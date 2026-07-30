# Fork CI setup (Terraform)

Configures a forked `linux-wallpaperengine` repo so `cmake.yml` runs on GitHub-hosted
runners: enables Actions (the "I understand my workflows..." fork banner), sets
default `GITHUB_TOKEN` workflow permissions to read/write (needed for the CodeQL
`security-events: write` step), and can optionally fire a `workflow_dispatch` run.

Provider: [`integrations/github`](https://registry.terraform.io/providers/integrations/github/latest).

## Usage

```bash
export GITHUB_TOKEN=<your PAT>   # needs repo + workflow scopes

cd .github/terraform
terraform init
terraform apply -var github_owner=<your-username>
```

To also trigger a build immediately:

```bash
terraform apply \
  -var github_owner=<your-username> \
  -var trigger_workflow_run=true \
  -var run_id=$(date +%s)
```

`run_id` just needs to change between applies to force another dispatch — reuse
the same value and Terraform treats it as a no-op.

## What's NOT covered

The PAT itself, and whether the fork is public (CodeQL/Advanced Security is
free on public repos, licensed on private ones) — both stay manual decisions.
