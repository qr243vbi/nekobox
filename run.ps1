param (
    [string]$Owner,
    [string]$Repo
)

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    Write-Host "GitHub CLI (gh) is not installed. Please install it from https://cli.github.com/" -ForegroundColor Red
    exit 1
}

if (-not $Owner -or -not $Repo) {
    Write-Host "Usage: .\run.ps1 -Owner <GitHub-Owner> -Repo <GitHub-Repo>" -ForegroundColor Yellow
    exit 1
}

$workflowRuns = gh api repos/$Owner/$Repo/actions/runs --paginate --jq '.workflow_runs[].id'

foreach ($runId in $workflowRuns) {
    Write-Host "Deleting workflow run ID: $runId"
    gh api repos/$Owner/$Repo/actions/runs/$runId -X DELETE | cat
}
gh cache delete --all

Write-Host "All workflow runs have been deleted."
