#!/usr/bin/env python3
"""Host-safe tests: package operations and builds use stub commands, never sudo."""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "build.sh"


class BuildScriptTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="lwe build test ")
        self.addCleanup(self.tmp.cleanup)
        self.work = Path(self.tmp.name)
        self.bin = self.work / "commands"
        self.bin.mkdir()
        self.log = self.work / "commands.log"
        self.env = {**os.environ, "PATH": str(self.bin) + os.pathsep + os.environ["PATH"],
                    "TEST_LOG": str(self.log)}
        self.stub("sudo", 'printf "UNEXPECTED SUDO\\n" >> "$TEST_LOG"; exit 99')

    def stub(self, name, body):
        p = self.bin / name
        p.write_text("#!/bin/bash\n" + body + "\n")
        p.chmod(0o755)

    def bash(self, code, success=True):
        result = subprocess.run(["bash", "-c", 'source "$1"; ' + code, "test", str(SCRIPT)],
                                env=self.env, text=True, capture_output=True)
        if success:
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
        return result

    def test_distribution_families(self):
        for ids, expected in [("nobara rhel centos fedora", "dnf"), ("ubuntu debian", "apt-get"),
                              ("cachyos arch", "pacman"), ("opensuse-tumbleweed suse", "zypper"),
                              ("void", "xbps-install"), ("custom ubuntu debian", "apt-get")]:
            with self.subTest(ids=ids):
                self.assertEqual(self.bash("manager_for " + ids).stdout, expected)
        self.bash("manager_for unknown", success=False)

    def test_installed_package_queries(self):
        self.stub("dpkg-query", '[[ ${@: -1} == present ]] || exit 1; printf "install ok installed"')
        self.stub("rpm", '[[ $1 == -q && $2 == --whatprovides && $3 == present ]]')
        self.stub("pacman", '[[ $1 == -T && $2 == present ]]')
        self.stub("xbps-query", '[[ ${@: -1} == present ]] || exit 1; printf installed')
        for manager in ["apt-get", "dnf", "dnf5", "pacman", "zypper", "xbps-install"]:
            self.bash(f"manager={manager}; installed present; ! installed absent")

    def test_package_plans(self):
        for manager in ["apt-get", "dnf", "dnf5", "pacman", "zypper", "xbps-install"]:
            code = f'''manager={manager}; dry_run=1; skip_deps=0; assume_yes=0
                package_list() {{ packages=(present missing); }}
                installed() {{ [[ $1 == present ]]; }}
                install_dependencies'''
            result = self.bash(code)
            commands = [x for x in result.stdout.splitlines() if x.startswith(" +")]
            self.assertTrue(commands)
            self.assertTrue(any("missing" in x for x in commands))
            self.assertFalse(any("present" in x for x in commands))
            self.assertFalse(self.log.exists(), "dry run must not execute sudo")
            if manager == "pacman":
                self.assertIn("-Syu", result.stdout)
                self.assertNotIn("--noconfirm", result.stdout)
            confirmed = self.bash(code.replace("assume_yes=0", "assume_yes=1"))
            self.assertIn("--noconfirm" if manager == "pacman" else "-y", confirmed.stdout)

    def test_all_installed_and_install_failure(self):
        self.bash('manager=dnf; dry_run=0; skip_deps=0; assume_yes=0; '
                  'package_list(){ packages=(a); }; installed(){ return 0; }; install_dependencies')
        self.assertFalse(self.log.exists())
        failed = self.bash('manager=dnf; dry_run=0; skip_deps=0; assume_yes=0; '
                           'package_list(){ packages=(a); }; installed(){ return 1; }; '
                           'root_run(){ return 42; }; install_dependencies; echo SHOULD_NOT_REACH', success=False)
        self.assertNotIn("SHOULD_NOT_REACH", failed.stdout)

    def test_link_creation_replacement_and_regular_file(self):
        output = self.work / "build with spaces" / "output"
        output.mkdir(parents=True)
        binary = output / "linux-wallpaperengine"
        binary.touch()
        dest = self.work / "new bin"
        code = (f'dry_run=0; no_link=0; build_dir={shlex.quote(str(output.parent))}; '
                f'bin_dir={shlex.quote(str(dest))}; install_link')
        self.bash(code)
        link = dest / "linux-wallpaperengine"
        self.assertTrue(link.is_symlink())
        self.assertEqual(link.resolve(), binary)
        self.bash(code)  # idempotent
        link.unlink()
        link.symlink_to(self.work / "gone")
        self.bash(code)  # broken symlink replacement
        self.assertEqual(link.resolve(), binary)
        link.unlink()
        link.write_text("keep existing executable")
        result = self.bash(code, success=False)
        self.assertIn("Refusing to replace", result.stderr)
        self.assertEqual(link.read_text(), "keep existing executable")
        self.assertFalse(self.log.exists())

    def test_build_workflow_and_failure_keeps_existing_link(self):
        # A fixture checkout exercises the actual CLI, quoting and ordering.
        repo = self.work / "source with spaces"
        repo.mkdir()
        shutil.copy2(SCRIPT, repo / "build.sh")
        shutil.copytree(ROOT / "kde/autostart", repo / "kde/autostart")
        self.env["XDG_CURRENT_DESKTOP"] = ""
        dest = self.work / "bin"
        dest.mkdir()
        link = dest / "linux-wallpaperengine"
        old = self.work / "old"
        old.touch()
        link.symlink_to(old)
        for tool in ["git", "make", "pkg-config", "wayland-scanner", "c++"]:
            self.stub(tool, 'printf "%s %s\\n" "${0##*/}" "$*" >> "$TEST_LOG"; cat >/dev/null </dev/null')
        self.stub("c++", 'cat >/dev/null')
        self.stub("cmake", '''printf 'cmake %s\\n' "$*" >> "$TEST_LOG"
if [[ $1 == --build ]]; then
    [[ ${FAIL_BUILD:-0} != 1 ]] || exit 42
    mkdir -p "$2/output"
    printf '#!/bin/bash\\nexit 0\\n' > "$2/output/linux-wallpaperengine"
    chmod +x "$2/output/linux-wallpaperengine"
fi''')
        args = ["bash", str(repo / "build.sh"), "--skip-deps", "--bin-dir", str(dest)]
        # Main refuses real root invocation. The host-safe function tests still
        # run as root in CI; this CLI integration test runs under an ordinary user.
        if os.geteuid() == 0:
            self.skipTest("CLI intentionally refuses root; run as a regular user")
        failed = subprocess.run(args, env={**self.env, "FAIL_BUILD": "1"}, text=True, capture_output=True)
        self.assertNotEqual(failed.returncode, 0, failed.stdout)
        self.assertEqual(link.resolve(), old)
        result = subprocess.run(args, env=self.env, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(link.resolve(), repo / "build/output/linux-wallpaperengine")
        self.assertIn("Autostart setup skipped", result.stdout)
        commands = self.log.read_text()
        self.assertIn("submodule update --init --recursive", commands)
        self.assertIn("-DCMAKE_BUILD_TYPE=Release", commands)
        self.assertIn("--target linux-wallpaperengine --parallel 2", commands)
        self.assertNotIn("UNEXPECTED SUDO", commands)

    def test_invalid_options(self):
        for args in [["--jobs", "0"], ["--jobs"], ["--build-dir"], ["--unknown"]]:
            result = subprocess.run(["bash", str(SCRIPT), *args], env=self.env, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.log.exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
