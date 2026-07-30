variable "github_owner" {
  description = "GitHub user/org that owns the fork (used as provider `owner`)."
  type        = string
}

variable "repository_name" {
  description = "Name of the forked repository."
  type        = string
  default     = "linux-wallpaperengine"
}

variable "trigger_workflow_run" {
  description = "Set true to fire a workflow_dispatch run of cmake.yml on apply."
  type        = bool
  default     = false
}

variable "workflow_ref" {
  description = "Branch/ref to run cmake.yml against when trigger_workflow_run is true."
  type        = string
  default     = "main"
}

variable "run_id" {
  description = "Arbitrary string that changes between applies to force a new workflow_dispatch call (e.g. a timestamp passed via -var)."
  type        = string
  default     = "manual"
}
