# Equivalent of the "I understand my workflows, go ahead and enable them"
# banner GitHub shows on a fresh fork's Actions tab.
resource "github_actions_repository_permissions" "this" {
  repository      = var.repository_name
  enabled         = true
  allowed_actions = "all"
}
