output "actions_settings_url" {
  description = "GitHub UI page for this repository's Actions settings."
  value       = "https://github.com/${var.github_owner}/${var.repository_name}/settings/actions"
}

output "actions_runs_url" {
  description = "GitHub UI page listing workflow runs."
  value       = "https://github.com/${var.github_owner}/${var.repository_name}/actions"
}
