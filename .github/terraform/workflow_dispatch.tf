# Optional: fire a workflow_dispatch run of cmake.yml so you don't have to
# click "Run workflow" in the UI. Imperative by nature (not real Terraform
# state) — only runs when trigger_workflow_run = true, and only re-runs when
# run_id changes between applies.
resource "null_resource" "trigger_cmake_workflow" {
  count = var.trigger_workflow_run ? 1 : 0

  triggers = {
    run_id = var.run_id
  }

  provisioner "local-exec" {
    command = <<-EOT
      curl -sS -X POST \
        -H "Authorization: Bearer $${GITHUB_TOKEN}" \
        -H "Accept: application/vnd.github+json" \
        "https://api.github.com/repos/${var.github_owner}/${var.repository_name}/actions/workflows/cmake.yml/dispatches" \
        -d '{"ref":"${var.workflow_ref}"}' \
        --fail-with-body
    EOT
  }

  depends_on = [
    github_actions_repository_permissions.this,
    null_resource.default_workflow_permissions,
  ]
}
