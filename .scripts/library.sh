#!/usr/bin/env bash

set -Eeuo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "$script_dir/.." && pwd)"
libraries_csv_file="$script_dir/libraries.csv"

cd -- "$project_root"

if [[ -t 1 && -z "${NO_COLOR:-}" ]]; then
    cyan=$'\033[36m'
    green=$'\033[32m'
    yellow=$'\033[33m'
    red=$'\033[31m'
    reset=$'\033[0m'
else
    cyan=""
    green=""
    yellow=""
    red=""
    reset=""
fi

info() {
    printf '%s%s%s\n' "$cyan" "$*" "$reset"
}

success() {
    printf '%s%s%s\n' "$green" "$*" "$reset"
}

warn() {
    printf '%s%s%s\n' "$yellow" "$*" "$reset"
}

error() {
    printf '%s%s%s\n' "$red" "$*" "$reset" >&2
}

die() {
    error "$*"
    exit 1
}

ensure_csv() {
    if [[ ! -f "$libraries_csv_file" ]]; then
        warn "'libraries.csv' file not found. Creating a new one..."
        printf '%s\n' '"Name","Version","Link","Project"' >"$libraries_csv_file"
    fi
}

decode_csv_field() {
    local value="${1%$'\r'}"
    value="${value#$'\xef\xbb\xbf'}"
    if [[ "$value" == \"*\" ]]; then
        value="${value#\"}"
        value="${value%\"}"
        value="${value//\"\"/\"}"
    fi
    printf '%s' "$value"
}

encode_csv_field() {
    local value="${1//\"/\"\"}"
    printf '"%s"' "$value"
}

declare -a library_names=()
declare -a library_versions=()
declare -a library_links=()
declare -a library_projects=()

load_libraries() {
    local name version link project remainder
    while IFS=',' read -r name version link project remainder; do
        name="$(decode_csv_field "$name")"
        version="$(decode_csv_field "$version")"
        link="$(decode_csv_field "$link")"
        project="$(decode_csv_field "$project")"

        [[ "$name" == "Name" || -z "$name" ]] && continue
        if [[ -n "${remainder:-}" ]]; then
            die "CSV fields containing commas are not supported by this Bash manager."
        fi

        library_names+=("$name")
        library_versions+=("$version")
        library_links+=("$link")
        library_projects+=("$project")
    done <"$libraries_csv_file"
}

write_libraries() {
    local temporary_file
    temporary_file="$(mktemp "$script_dir/.libraries.csv.XXXXXX")"
    printf '%s\n' '"Name","Version","Link","Project"' >"$temporary_file"

    local index
    for index in "${!library_names[@]}"; do
        printf '%s,%s,%s,%s\n' \
            "$(encode_csv_field "${library_names[$index]}")" \
            "$(encode_csv_field "${library_versions[$index]}")" \
            "$(encode_csv_field "${library_links[$index]}")" \
            "$(encode_csv_field "${library_projects[$index]}")" \
            >>"$temporary_file"
    done

    mv -- "$temporary_file" "$libraries_csv_file"
}

choice=""
prompt_for_choice() {
    local message="$1"
    shift
    local -a choices=("$@")

    ((${#choices[@]} > 0)) || die "No choices are available."

    printf '%s\n' "$message"
    local index
    for index in "${!choices[@]}"; do
        printf '%d. %s\n' "$((index + 1))" "${choices[$index]}"
    done

    local answer
    while true; do
        read -r -p "Enter the number of your choice: " answer || die "Input cancelled."
        if [[ "$answer" =~ ^[0-9]+$ ]] && ((answer >= 1 && answer <= ${#choices[@]})); then
            choice="${choices[$((answer - 1))]}"
            return
        fi
        warn "Invalid choice."
    done
}

tag_exists() {
    local wanted="$1"
    shift
    local tag
    for tag in "$@"; do
        [[ "$tag" == "$wanted" ]] && return 0
    done
    return 1
}

safe_library_name() {
    local name="$1"
    [[ -n "$name" && "$name" != "." && "$name" != ".." && "$name" != */* ]]
}

find_library_by_name() {
    local project="$1"
    local name="$2"
    found_index=-1

    local index
    for index in "${!library_names[@]}"; do
        if [[ "${library_projects[$index]}" == "$project" && "${library_names[$index]}" == "$name" ]]; then
            found_index="$index"
            return 0
        fi
    done
    return 1
}

find_library_by_link() {
    local project="$1"
    local link="$2"
    found_index=-1

    local index
    for index in "${!library_links[@]}"; do
        if [[ "${library_projects[$index]}" == "$project" && "${library_links[$index]}" == "$link" ]]; then
            found_index="$index"
            return 0
        fi
    done
    return 1
}

select_library_name() {
    local project="$1"
    local -a names=()
    local index

    for index in "${!library_names[@]}"; do
        if [[ "${library_projects[$index]}" == "$project" ]]; then
            names+=("${library_names[$index]}")
        fi
    done

    if ((${#names[@]} == 0)); then
        warn "No libraries found for project '$project'."
        selected_library_name=""
        return 1
    fi

    info "Libraries in project '$project':"
    for index in "${!library_names[@]}"; do
        if [[ "${library_projects[$index]}" == "$project" ]]; then
            printf '  - %s (%s)\n' "${library_names[$index]}" "${library_versions[$index]}"
        fi
    done

    prompt_for_choice "Select a library:" "${names[@]}"
    selected_library_name="$choice"
}

update_library() {
    local project="$1"
    local library_name="${2:-}"

    if ((${#library_names[@]} == 0)); then
        warn "No libraries to update."
        return
    fi

    if [[ -z "$library_name" ]]; then
        select_library_name "$project" || return
        library_name="$selected_library_name"
    fi

    if ! find_library_by_name "$project" "$library_name"; then
        error "Library '$library_name' not found in project '$project'."
        return
    fi

    local index="$found_index"
    local submodule_path="$project/vendor/${library_names[$index]}"
    if [[ ! -d "$submodule_path" ]]; then
        error "Submodule path '$submodule_path' not found. Cannot update version."
        return
    fi

    info "Updating library:"
    printf '  Name: %s\n' "${library_names[$index]}"
    printf '  Current Version: %s\n' "${library_versions[$index]}"
    printf '  Project: %s\n' "${library_projects[$index]}"
    printf '  Path: %s\n' "$submodule_path"

    success "Fetching latest versions..."
    if ! git -C "$submodule_path" fetch --tags; then
        error "Failed to fetch tags for '${library_names[$index]}'."
        return
    fi

    local -a tags=()
    mapfile -t tags < <(git -C "$submodule_path" tag)
    if ((${#tags[@]} == 0)); then
        error "No version tags found in the repository. Cannot update."
        return
    fi

    info "Available versions:"
    printf '  %s\n' "${tags[@]}"
    warn "Current version: ${library_versions[$index]}"

    local selected_version_tag
    read -r -p "Enter the version name (or press Enter to keep current): " selected_version_tag || return
    if [[ -z "$selected_version_tag" ]]; then
        info "Keeping current version '${library_versions[$index]}'."
        return
    fi
    if ! tag_exists "$selected_version_tag" "${tags[@]}"; then
        error "Invalid version tag selected. No changes made."
        return
    fi
    if [[ "$selected_version_tag" == "${library_versions[$index]}" ]]; then
        warn "Selected version is the same as current version. No changes made."
        return
    fi

    local previous_version="${library_versions[$index]}"
    if git -C "$submodule_path" checkout "tags/$selected_version_tag"; then
        library_versions[index]="$selected_version_tag"
        success "Library '${library_names[$index]}' updated from '$previous_version' to '$selected_version_tag'."
    else
        error "Failed to checkout version '$selected_version_tag'."
    fi
}

add_library() {
    local project="$1"
    local library_link="${2:-}"

    if [[ -z "$library_link" ]]; then
        read -r -p "Enter git library link: " library_link || die "Input cancelled."
    fi
    [[ -n "$library_link" ]] || die "Library link cannot be empty."

    if find_library_by_link "$project" "$library_link"; then
        local existing_index="$found_index"
        warn "Library '${library_names[$existing_index]}' already exists in the project."
        printf 'Version: %s\n' "${library_versions[$existing_index]}"
        printf 'Link:    %s\n' "${library_links[$existing_index]}"
        printf 'Project: %s\n' "${library_projects[$existing_index]}"

        local update_version
        read -r -p "Do you want to update the version? (y/n): " update_version || return
        if [[ "${update_version,,}" == "y" ]]; then
            update_library "$project" "${library_names[$existing_index]}"
        else
            info "No changes made to '${library_names[$existing_index]}'."
        fi
        return
    fi

    local normalized_link="${library_link%/}"
    local library_name="${normalized_link##*/}"
    library_name="${library_name%.git}"
    safe_library_name "$library_name" || die "Could not derive a safe library name from '$library_link'."

    local vendor_path="$project/vendor"
    local submodule_path="$vendor_path/$library_name"
    mkdir -p -- "$vendor_path"

    if [[ -e "$submodule_path" ]]; then
        error "Directory '$submodule_path' already exists. Please remove it first."
        return
    fi

    success "Adding Git submodule for '$library_name'..."
    if ! git submodule add "$library_link" "$submodule_path"; then
        error "Failed to add Git submodule. Please check the repository URL."
        return
    fi

    git -C "$submodule_path" fetch --tags || warn "Could not refresh tags; using the checked-out branch."
    local -a tags=()
    mapfile -t tags < <(git -C "$submodule_path" tag)

    local selected_version_tag
    if ((${#tags[@]} == 0)); then
        warn "No version tags found in the repository. Using the checked-out branch."
        selected_version_tag="$(git -C "$submodule_path" branch --show-current)"
        selected_version_tag="${selected_version_tag:-main}"
    else
        info "Available versions:"
        printf '  %s\n' "${tags[@]}"
        read -r -p "Enter the version name: " selected_version_tag || selected_version_tag=""

        if tag_exists "$selected_version_tag" "${tags[@]}"; then
            if ! git -C "$submodule_path" checkout "tags/$selected_version_tag"; then
                error "Failed to checkout '$selected_version_tag'. Keeping the current branch."
                selected_version_tag="$(git -C "$submodule_path" branch --show-current)"
                selected_version_tag="${selected_version_tag:-main}"
            fi
        else
            warn "Invalid version tag selected. Using the checked-out branch."
            selected_version_tag="$(git -C "$submodule_path" branch --show-current)"
            selected_version_tag="${selected_version_tag:-main}"
        fi
    fi

    library_names+=("$library_name")
    library_versions+=("$selected_version_tag")
    library_links+=("$library_link")
    library_projects+=("$project")
    success "Library '$library_name' added successfully with version '$selected_version_tag'."
}

remove_library() {
    local project="$1"
    local library_name="${2:-}"

    if ((${#library_names[@]} == 0)); then
        warn "No libraries to remove."
        return
    fi

    if [[ -z "$library_name" ]]; then
        select_library_name "$project" || return
        library_name="$selected_library_name"
    fi

    if ! find_library_by_name "$project" "$library_name"; then
        error "Library '$library_name' not found in project '$project'."
        return
    fi

    local index="$found_index"
    safe_library_name "${library_names[$index]}" || die "Refusing unsafe library path."

    local submodule_path="$project/vendor/${library_names[$index]}"
    case "$submodule_path" in
        engine/vendor/*|tests/vendor/*) ;;
        *) die "Refusing to remove unsafe path '$submodule_path'." ;;
    esac

    warn "About to remove:"
    printf '  Name: %s\n' "${library_names[$index]}"
    printf '  Version: %s\n' "${library_versions[$index]}"
    printf '  Project: %s\n' "${library_projects[$index]}"
    printf '  Path: %s\n' "$submodule_path"

    local confirm
    read -r -p "Are you sure you want to remove this library? (y/n): " confirm || return
    if [[ "${confirm,,}" != "y" ]]; then
        info "Removal cancelled."
        return
    fi

    if [[ -e "$submodule_path" ]]; then
        success "Removing Git submodule..."
        git submodule deinit -f -- "$submodule_path" || warn "Submodule was already deinitialized."
        git rm -f -- "$submodule_path" || warn "Git could not remove '$submodule_path' from the index."
        if [[ -e "$submodule_path" ]]; then
            rm -rf -- "$submodule_path"
        fi

        local git_modules_path="$project_root/.git/modules/$submodule_path"
        if [[ -d "$git_modules_path" ]]; then
            rm -rf -- "$git_modules_path"
        fi
    else
        warn "Submodule path '$submodule_path' not found on disk."
    fi

    unset 'library_names[index]'
    unset 'library_versions[index]'
    unset 'library_links[index]'
    unset 'library_projects[index]'
    library_names=("${library_names[@]}")
    library_versions=("${library_versions[@]}")
    library_links=("${library_links[@]}")
    library_projects=("${library_projects[@]}")
    success "Removed '$library_name' from project '$project'."
}

info "=== Library Manager ==="
ensure_csv
load_libraries

action="${1:-}"
if [[ -z "$action" ]]; then
    prompt_for_choice "Select action:" "Add" "Update" "Remove"
    action="$choice"
fi
action="${action,,}"

project="${2:-}"
if [[ -z "$project" ]]; then
    prompt_for_choice "Select project:" "tests" "engine"
    project="$choice"
fi
case "$project" in
    tests|engine) ;;
    *) die "Invalid project '$project'. Use 'tests' or 'engine'." ;;
esac

case "$action" in
    add)
        add_library "$project" "${3:-}"
        ;;
    update)
        update_library "$project" "${3:-}"
        ;;
    remove)
        remove_library "$project" "${3:-}"
        ;;
    *)
        die "Invalid action '$action'. Use add, update, or remove."
        ;;
esac

write_libraries
printf '\n'
info "'$libraries_csv_file' file updated successfully!"
info "Current libraries:"
while IFS= read -r line; do
    printf '%s\n' "$line"
done <"$libraries_csv_file"
