echo "${BASH_SOURCE[0]}"
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    echo "Script is executed directly."
else
    echo "Script is sourced."
fi
