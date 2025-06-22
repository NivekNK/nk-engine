Write-Host "=== Library Manager ===" -ForegroundColor Cyan

$librariesCsvFile = ".scripts/libraries.csv"

if (-not (Test-Path $librariesCsvFile)) {
    Write-Host "'libraries.csv' file not found. Creating a new one..." -ForegroundColor Yellow
    @"
Name,Version,Link,Project
"@ | Out-File -Encoding UTF8 $librariesCsvFile
}

# Ensure we always have an array, even if CSV is empty
$libraries = @(Import-Csv $librariesCsvFile)
if ($null -eq $libraries) {
    $libraries = @()
}

function PromptForChoice {
    param (
        [string]$Message,
        [array]$Choices
    )
    Write-Host $Message
    for ($i = 0; $i -lt $Choices.Count; $i++) {
        Write-Host "$($i + 1). $($Choices[$i])"
    }

    do {
        $choice = Read-Host "Enter the number of your choice"
        $index = [int]$choice - 1
    } while ($index -lt 0 -or $index -ge $Choices.Count)

    return $Choices[$index]
}

function Update-Library {
    param(
        [array]$Libraries,
        [string]$Project,
        [string]$LibraryName = $null
    )

    if ($Libraries.Count -eq 0) {
        Write-Host "No libraries to update." -ForegroundColor Yellow
        return $Libraries
    }

    # Filter libraries by project
    $projectLibraries = @($Libraries | Where-Object { $_.Project -eq $Project })
    
    if ($projectLibraries.Count -eq 0) {
        Write-Host "No libraries found for project '$Project'." -ForegroundColor Yellow
        return $Libraries
    }

    # If no library name specified, let user choose
    if ([string]::IsNullOrWhiteSpace($LibraryName)) {
        Write-Host "Libraries in project '$Project':" -ForegroundColor Cyan
        $projectLibraries | ForEach-Object { 
            Write-Host "  - $($_.Name) ($($_.Version))" 
        }

        $names = $projectLibraries | ForEach-Object { $_.Name }
        $LibraryName = PromptForChoice "Select a library to update:" $names
    }

    # Find the specific library
    $libraryToUpdate = $projectLibraries | Where-Object { $_.Name -eq $LibraryName }
    
    if (-not $libraryToUpdate) {
        Write-Host "Library '$LibraryName' not found in project '$Project'." -ForegroundColor Red
        return $Libraries
    }

    $submodulePath = "$Project/vendor/$($libraryToUpdate.Name)"

    Write-Host "Updating library:" -ForegroundColor Cyan
    Write-Host "  Name: $($libraryToUpdate.Name)"
    Write-Host "  Current Version: $($libraryToUpdate.Version)"
    Write-Host "  Project: $($libraryToUpdate.Project)"
    Write-Host "  Path: $submodulePath"

    # Check if submodule path exists
    if (-not (Test-Path $submodulePath)) {
        Write-Host "Submodule path '$submodulePath' not found. Cannot update version." -ForegroundColor Red
        return $Libraries
    }

    # Fetch latest tags
    Write-Host "Fetching latest versions..." -ForegroundColor Green
    Push-Location $submodulePath
    git fetch --tags > $null 2>&1
    $tags = git tag | ForEach-Object { $_.Trim() }
    Pop-Location

    if (-not $tags) {
        Write-Host "No version tags found in the repository. Cannot update." -ForegroundColor Red
        return $Libraries
    }

    Write-Host "Available versions:" -ForegroundColor Cyan
    $tags | ForEach-Object { Write-Host "  $_" }
    Write-Host "Current version: $($libraryToUpdate.Version)" -ForegroundColor Yellow
    
    $selectedVersionTag = Read-Host "Enter the version name (or press Enter to keep current)"

    # If user pressed Enter without input, keep current version
    if ([string]::IsNullOrWhiteSpace($selectedVersionTag)) {
        Write-Host "Keeping current version '$($libraryToUpdate.Version)'." -ForegroundColor Cyan
        return $Libraries
    }

    if (-not $tags -contains $selectedVersionTag) {
        Write-Host "Invalid version tag selected. No changes made." -ForegroundColor Red
        return $Libraries
    }

    # Check if it's the same version
    if ($selectedVersionTag -eq $libraryToUpdate.Version) {
        Write-Host "Selected version is the same as current version. No changes made." -ForegroundColor Yellow
        return $Libraries
    }

    # Update the library version
    $libraryToUpdate.Version = $selectedVersionTag

    # Checkout the new version
    Push-Location $submodulePath
    $checkoutResult = git checkout "tags/$selectedVersionTag" 2>&1
    Pop-Location

    if ($LASTEXITCODE -eq 0) {
        Write-Host "Library '$($libraryToUpdate.Name)' updated from '$($libraryToUpdate.Version)' to '$selectedVersionTag'." -ForegroundColor Green
    } else {
        Write-Host "Failed to checkout version '$selectedVersionTag'. Error: $checkoutResult" -ForegroundColor Red
        # Revert the version change
        $libraryToUpdate.Version = $libraryToUpdate.Version
    }

    return $Libraries
}

function Add-Library {
    param(
        [array]$Libraries,
        [string]$Project,
        [string]$LibraryLink
    )

    $existingLibrary = $Libraries | Where-Object { 
        $_.Link -eq $LibraryLink -and $_.Project -eq $Project 
    }

    if ($existingLibrary) {
        Write-Host "Library '$($existingLibrary.Name)' already exists in the project." -ForegroundColor Yellow
        Write-Host "Version: $($existingLibrary.Version)"
        Write-Host "Link   : $($existingLibrary.Link)"
        Write-Host "Project: $($existingLibrary.Project)"

        $updateVersion = Read-Host "Do you want to update the version? (y/n)"
        if ($updateVersion -eq 'y') {
            # Reuse the Update-Library function
            $Libraries = Update-Library -Libraries $Libraries -Project $Project -LibraryName $existingLibrary.Name
        } else {
            Write-Host "No changes made to '$($existingLibrary.Name)'." -ForegroundColor Cyan
        }
    } else {
        # Extract library name from URL
        $libraryName = Split-Path -Leaf $LibraryLink
        if ($libraryName.EndsWith('.git')) {
            $libraryName = $libraryName.Substring(0, $libraryName.Length - 4)
        }
        
        $vendorPath = "$Project/vendor"
        $submodulePath = "$vendorPath/$libraryName"

        # Create vendor directory if it doesn't exist
        if (-not (Test-Path -Path $vendorPath)) {
            New-Item -ItemType Directory -Path $vendorPath -Force | Out-Null
            Write-Host "$Project 'vendor' folder created!" -ForegroundColor Green
        }

        # Check if submodule already exists on disk
        if (Test-Path $submodulePath) {
            Write-Host "Directory '$submodulePath' already exists. Please remove it first." -ForegroundColor Red
            return $Libraries
        }

        Write-Host "Adding Git submodule for '$libraryName'..." -ForegroundColor Green
        $gitProcess = Start-Process git -ArgumentList "submodule add $LibraryLink $submodulePath" -NoNewWindow -Wait -PassThru
        
        if ($gitProcess.ExitCode -ne 0) {
            Write-Host "Failed to add Git submodule. Please check the repository URL." -ForegroundColor Red
            return $Libraries
        }

        # Fetch tags and let user select version
        if (Test-Path $submodulePath) {
            Push-Location $submodulePath
            git fetch --tags > $null 2>&1
            $tags = git tag | ForEach-Object { $_.Trim() }
            Pop-Location

            if (-not $tags) {
                Write-Host "No version tags found in the repository. Using default branch." -ForegroundColor Yellow
                $selectedVersionTag = "main"
            } else {
                Write-Host "Available versions:" -ForegroundColor Cyan
                $tags | ForEach-Object { Write-Host "  $_" }
                $selectedVersionTag = Read-Host "Enter the version name"

                if (-not $tags -contains $selectedVersionTag) {
                    Write-Host "Invalid version tag selected. Using default branch." -ForegroundColor Yellow
                    $selectedVersionTag = "main"
                } else {
                    Push-Location $submodulePath
                    git checkout "tags/$selectedVersionTag" > $null 2>&1
                    Pop-Location
                }
            }

            # Add new library to the array
            $Libraries = @($Libraries)
            $newLibrary = [PSCustomObject]@{
                Name    = $libraryName
                Version = $selectedVersionTag
                Link    = $LibraryLink
                Project = $Project
            }
            $Libraries += $newLibrary

            Write-Host "Library '$libraryName' added successfully with version '$selectedVersionTag'." -ForegroundColor Green
        } else {
            Write-Host "Failed to create submodule directory." -ForegroundColor Red
        }
    }

    return $Libraries
}

function Remove-Library {
    param(
        [array]$Libraries,
        [string]$Project
    )

    if ($Libraries.Count -eq 0) {
        Write-Host "No libraries to remove." -ForegroundColor Yellow
        return $Libraries
    }

    # Filter libraries by project
    $projectLibraries = @($Libraries | Where-Object { $_.Project -eq $Project })
    
    if ($projectLibraries.Count -eq 0) {
        Write-Host "No libraries found for project '$Project'." -ForegroundColor Yellow
        return $Libraries
    }

    Write-Host "Libraries in project '$Project':" -ForegroundColor Cyan
    $projectLibraries | ForEach-Object { 
        Write-Host "  - $($_.Name) ($($_.Version))" 
    }

    # Let user pick a library by Name
    $names = $projectLibraries | ForEach-Object { $_.Name }
    $toRemoveName = PromptForChoice "Select a library to remove:" $names

    $lib = $projectLibraries | Where-Object { $_.Name -eq $toRemoveName }
    $submodulePath = "$Project/vendor/$($lib.Name)"

    Write-Host "About to remove:" -ForegroundColor Yellow
    Write-Host "  Name: $($lib.Name)"
    Write-Host "  Version: $($lib.Version)" 
    Write-Host "  Project: $($lib.Project)"
    Write-Host "  Path: $submodulePath"
    
    $confirm = Read-Host "Are you sure you want to remove this library? (y/n)"
    if ($confirm -ne 'y') {
        Write-Host "Removal cancelled." -ForegroundColor Cyan
        return $Libraries
    }

    # Check if submodule path exists
    if (Test-Path $submodulePath) {
        Write-Host "Removing Git submodule..." -ForegroundColor Green
        
        # 1) Deinitialize submodule
        git submodule deinit -f -- $submodulePath 2>$null

        # 2) Remove from git index and .gitmodules
        git rm -f $submodulePath 2>$null
        
        # 3) Remove the physical directory
        if (Test-Path $submodulePath) {
            Remove-Item -Recurse -Force $submodulePath
        }

        # 4) Remove from .git/modules if it exists
        $gitModulesPath = ".git/modules/$submodulePath"
        if (Test-Path $gitModulesPath) {
            Remove-Item -Recurse -Force $gitModulesPath
        }

        Write-Host "Git submodule removed from disk." -ForegroundColor Green
    } else {
        Write-Host "Submodule path '$submodulePath' not found on disk." -ForegroundColor Yellow
    }

    # Remove from libraries array
    $newList = @()
    foreach ($library in $Libraries) {
        if (-not ($library.Name -eq $toRemoveName -and $library.Project -eq $Project)) {
            $newList += $library
        }
    }

    Write-Host "Removed '$toRemoveName' from project '$Project'." -ForegroundColor Green
    Write-Host "Libraries remaining: $($newList.Count)" -ForegroundColor Cyan
    
    return $newList
}

# Main script logic
$actions = @("Add", "Update", "Remove")
$action = PromptForChoice "Select action:" $actions

$projects = @("tests", "engine")
$project = PromptForChoice "Select project:" $projects

switch ($action) {
    "Add" {
        $libraryLink = Read-Host "Enter git library link"
        if ([string]::IsNullOrWhiteSpace($libraryLink)) {
            Write-Host "Library link cannot be empty." -ForegroundColor Red
            exit 1
        }
        $libraries = Add-Library -Libraries $libraries -Project $project -LibraryLink $libraryLink
    }
    "Update" {
        $libraries = Update-Library -Libraries $libraries -Project $project
    }
    "Remove" {
        $libraries = Remove-Library -Libraries $libraries -Project $project
    }
}

# Ensure we have an array before exporting
$libraries = @($libraries)

# Export CSV with proper formatting
if ($libraries.Count -gt 0) {
    $libraries | Export-Csv -Path $librariesCsvFile -NoTypeInformation -Encoding UTF8
    Write-Host "`n'$librariesCsvFile' file updated successfully!" -ForegroundColor Cyan
    Write-Host "Current libraries:" -ForegroundColor Cyan
    Get-Content "$librariesCsvFile"
} else {
    # If no libraries, create empty CSV with headers
    @"
Name,Version,Link,Project
"@ | Out-File -Encoding UTF8 $librariesCsvFile
    Write-Host "`nAll libraries removed. CSV reset to headers only." -ForegroundColor Cyan
}
