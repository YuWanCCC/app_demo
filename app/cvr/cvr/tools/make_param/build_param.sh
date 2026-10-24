#!/bin/bash
set -e

cwd=$(dirname $(readlink -f $0))
cd ${cwd}
CMD_VERSION="1.0.0"

function msg_info()
{
    echo -e "[$(basename $0):info] $1"
}

function msg_warn()
{
    echo -e "[$(basename $0):warn] $1"
}

function msg_error()
{
    echo -e "[$(basename $0):error] $1"
}

function msg_help()
{
    echo "command format:"
    echo "    `basename $cwd`"
    echo "        -v|--version         (Optional) view this version."
    echo "        -h|--help            (Optional) view this help message."
    echo "        -c|--clean           (Optional) clean."
    echo "        --ini_dir            (Required) input ini files directory."
    echo "        --output             (Optional) output directory for param.img"
    exit 0
}

function chk_param()
{
    BUILD_PARAM_PARAMS="-v --version -h --help -c --clean --ini_dir --output"

    while [ $# -ne 0 ]
    do
        case $1 in
            -v|--version)
                msg_info "V$CMD_VERSION"
                exit 0
                ;;
            -h|--help)
                msg_help
                ;;
            -c|--clean)
                make clean
                rm -f param.img
                exit 0
                ;;
            --ini_dir)
                INI_DIR="$2"
                shift
                ;;
            --output)
                OUTPUT_DIR="$2"
                shift
                ;;
            *)
                ;;
        esac
        shift
    done

    # Check required parameters
    if [ -z "$INI_DIR" ]; then
        msg_error "Please specify ini files directory with --ini_dir"
        exit 1
    fi

    if [ ! -d "$INI_DIR" ]; then
        msg_error "INI directory $INI_DIR does not exist"
        exit 1
    fi
}

# Parse parameters
chk_param $@

msg_info "========================================"
msg_info "INI_DIR=$INI_DIR"
msg_info "OUTPUT_DIR=$OUTPUT_DIR"
msg_info "========================================"

# Build make_param tool
make

# Set default output directory if not specified
if [ -z "$OUTPUT_DIR" ]; then
    OUTPUT_DIR="."
fi

# Generate param.img using make_param_host
./make_param_host "--ini_dir" "${INI_DIR}" "--output" "${OUTPUT_DIR}"

if [ $? -eq 0 ]; then
    msg_info "Successfully generated param.img"
    exit 0
else
    msg_error "Failed to generate param.img"
    exit 1
fi