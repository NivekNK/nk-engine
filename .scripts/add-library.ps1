Write-Host "=== Add Library To Project ===" -ForegroundColor Cyan

$librariesCsvFile = ".scripts/libraries.csv"

if (-not (Test-Path $librariesCsvFile)) {
    Write-Host "'libraries.csv' file not found. Creating a new one..." -ForegroundColor Yellow
    @"
Name,Version,Link,Project
"@ | Out-File -Encoding UTF8 $librariesCsvFile
}

$libraries = Import-Csv $librariesCsvFile

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

$libraryLink = Read-Host "Enter git library link"
$projects = @("tests", "engine")
$project = PromptForChoice "Select project:" $projects

$existingLibrary = $libraries | Where-Object { $_.Link -eq $libraryLink }

if ($existingLibrary) {
    Write-Host "Library '$($existingLibrary.Name)' already exists in the project." -ForegroundColor Yellow
    Write-Host "Version: $($existingLibrary.Version)"
    Write-Host "Link   : $($existingLibrary.Link)"

    $submodulePath = "$project/vendor/$($existingLibrary.Name)"

    $updateVersion = Read-Host "Do you want to update the version? (y/n)"
    if ($updateVersion -eq 'y') {
        Push-Location $submodulePath
        git fetch --tags > NUL 2>&1
        $tags = git tag | ForEach-Object { $_.Trim() }
        Pop-Location

        if (-not $tags) {
            Write-Host "No version tags found in the repository. Exiting." -ForegroundColor Red
            exit 1
        }

        Write-Host "Available versions:" -ForegroundColor Cyan
        $tags | ForEach-Object { Write-Host $_ }
        $selectedVersionTag = Read-Host "Enter the version name"

        if (-not $tags -contains $selectedVersionTag) {
            Write-Host "Invalid version tag selected. Exiting." -ForegroundColor Red
            exit 1
        }

        $existingLibrary.Version = $selectedVersionTag

        Push-Location $submodulePath
        git checkout "tags/$selectedVersionTag"
        Pop-Location

        Write-Host "Library '$($existingLibrary.Name)' version updated to '$selectedVersionTag' and submodule tag checked out." -ForegroundColor Green
    } else {
        Write-Host "No changes made to '$($existingLibrary.Name)'." -ForegroundColor Cyan
    }
} else {
    $libraryName = "$(Split-Path -Leaf $libraryLink)"
    $vendorPath = "$project/vendor"
    $submodulePath = "$vendorPath/$libraryName"

    if (-not (Test-Path -Path $vendorPath)) {
        New-Item -ItemType Directory -Path $vendorPath -Force | Out-Null
        Write-Host "$project 'vendor' folder created!"
    }

    Write-Host "Adding Git submodule for '$libraryName'..." -ForegroundColor Green
    Start-Process git -ArgumentList "submodule add $libraryLink $submodulePath" -NoNewWindow -Wait

    Push-Location $submodulePath
    git fetch --tags > NUL 2>&1
    $tags = git tag | ForEach-Object { $_.Trim() }
    Pop-Location

    if (-not $tags) {
        Write-Host "No version tags found in the repository. Exiting." -ForegroundColor Red
        exit 1
    }

    Write-Host "Available versions:" -ForegroundColor Cyan
    $tags | ForEach-Object { Write-Host $_ }
    $selectedVersionTag = Read-Host "Enter the version name"

    if (-not $tags -contains $selectedVersionTag) {
        Write-Host "Invalid version tag selected. Exiting." -ForegroundColor Red
        exit 1
    }

    $libraries = @($libraries)
    $newLibrary = [PSCustomObject]@{
        Name    = $libraryName
        Version = $selectedVersionTag
        Link    = $libraryLink
        Project = $project
    }
    $libraries += $newLibrary

    Push-Location $submodulePath
    git checkout "tags/$selectedVersionTag"
    Pop-Location

    Write-Host "Library '$libraryName' added to CSV and submodule initialized with tag '$selectedVersionTag'." -ForegroundColor Green
}

$libraries | Export-Csv -Path $librariesCsvFile -NoTypeInformation -Encoding UTF8
Write-Host "'$librariesCsvFile' file updated successfully!" -ForegroundColor Cyan

Get-Content "$librariesCsvFile"