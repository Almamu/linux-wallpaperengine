# No provider resource covers repo-scoped default workflow permissions for a
# personal (non-org) repository — github_actions_organization_workflow_permissions
# only applies to orgs. Call the REST API directly instead, so the CodeQL step's
# `security-events: write` permission isn't capped by a read-only default.
# Requires GITHUB_TOKEN to be exported in the shell running `terraform apply`.
resource "null_resource" "default_workflow_permissions" {
  triggers = {
    repository = var.repository_name
  }

  provisioner "local-exec" {
    command = <<-EOT
      curl -sS -X PUT \
        -H "Authorization: Bearer $${GITHUB_TOKEN}" \
        -H "Accept: application/vnd.github+json" \
        "https://api.github.com/repos/${var.github_owner}/${var.repository_name}/actions/permissions/workflow" \
        -d '{"default_workflow_permissions":"write","can_approve_pull_request_reviews":false}' \
        --fail-with-body
    EOT
  }

  depends_on = [github_actions_repository_permissions.this]
}
