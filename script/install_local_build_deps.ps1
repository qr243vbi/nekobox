# Optional: install minimal tools for LOCAL build (still need Qt + vcpkg manually).
# Run PowerShell AS ADMINISTRATOR:
#   Set-ExecutionPolicy -Scope Process Bypass
#   .\script\install_local_build_deps.ps1

$ErrorActionPreference = "Stop"


$packages = @(
    @{ Id = "GitHub.cli"; Name = "GitHub CLI" },
    @{ Id = "Kitware.CMake"; Name = "CMake" },
    @{ Id = "Ninja-build.Ninja"; Name = "Ninja" },
    @{ Id = "Microsoft.VisualStudio.2022.BuildTools"; Name = "VS 2022 Build Tools" }
)

foreach ($pkg in $packages) {
    Write-Host "-> $($pkg.Name)" -ForegroundColor Yellow
    if ($pkg.Id -eq "Microsoft.VisualStudio.2022.BuildTools") {
        winget install --id $pkg.Id -e --accept-package-agreements --accept-source-agreements `
            --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    } else {
        winget install --id $pkg.Id -e --accept-package-agreements --accept-source-agreements
    }
}
