# SPDX-License-Identifier: GPL-3.0-or-later
import subprocess, tempfile, pathlib, os, shutil, sys

# Run from the repository root under dbus-run-session.
# Pass the compiled kde/tests/mouse-bridge.cpp executable as the only argument.
server = subprocess.Popen([sys.argv[1], '--serve'], stdout=subprocess.PIPE, text=True)
try:
    service = server.stdout.readline().strip()
    with tempfile.TemporaryDirectory(prefix='lwe-qml-test-') as d:
        shutil.copy('kde/plasma/contents/ui/MouseRelay.qml', d)
        pathlib.Path(d, 'tst_transport.qml').write_text('''import QtQuick
import QtTest
Item {
 width: 400; height: 300
 MouseRelay { id: relay; forwarding: true; targetsJson: '[{"x":0,"y":294,"service":"SERVICE"}]' }
 TestCase { name: "QmlBusTransport"; when: windowShown
  function test_send() {
   relay.leftDown = true
   relay.send(false)
   tryCompare(relay, "inFlight", 0)
   compare(relay.failed, false)
   relay.release()
   tryCompare(relay, "inFlight", 0)
   compare(relay.failed, false)
  }
 }
}'''.replace('SERVICE', service))
        subprocess.run(['qmltestrunner-qt6','-input',d],env={**os.environ,'QT_QPA_PLATFORM':'offscreen'},check=True)
finally:
    server.terminate()
    server.wait()
