function print_help
{
    echo ""
    echo "=== ProcessGoldenImages.sh ==="
    echo ""
    echo "Required variables:"
    echo ""
    echo "  DILIGENT_BUILD_DIR  - Path to the build directory"
    echo ""
    echo "Optional variables:"
    echo ""
    echo "  GOLDEN_IMAGE_WIDTH  - Golden image width (Default: 512)"
    echo "  GOLDEN_IMAGE_HEIGHT - Golden image height (Default: 512)"
    echo ""
    echo "Command line format:"
    echo ""
    echo "  ProcessGoldenImages.sh golden_images_dir golden_img_mode test_modes"
    echo ""
    echo "    golden_images_dir - Path to the golden images directory"
    echo "    golden_img_mode   - golden image processing mode (capture, compare, or compare_update)"
    echo "    test_modes        - list of test modes (e.g. \"--mode gl\" \"--mode vk --adapter sw\")"
    echo "" 
    echo "Example:"
    echo "  export DILIGENT_BUILD_DIR=/git/DiligentEngine/build"
    echo "  bash ProcessGoldenImages.sh /git/DiligentTestData/GoldenImages compare \"--mode gl\" \"--mode vk --adapter sw\""
}


RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No color

if [[ $# -lt 3 ]]; then
    printf "${RED}At least three arguments are required${NC}\n"
    print_help
    exit 1
fi


if [[ "$DILIGENT_BUILD_DIR" == "" ]]; then
    printf "${RED}Required DILIGENT_BUILD_DIR variable is not set${NC}\n"
    print_help
    exit 1
fi

if [[ "$GOLDEN_IMAGE_WIDTH" == "" ]]; then
    GOLDEN_IMAGE_WIDTH=512
fi

if [[ "$GOLDEN_IMAGE_HEIGHT" == "" ]]; then
    GOLDEN_IMAGE_HEIGHT=512
fi

golden_images_dir=$1
shift

golden_img_mode=$1
shift

if [[ !("$golden_img_mode" == "capture" || "$golden_img_mode" == "compare" || "$golden_img_mode" == "compare_update") ]]; then
    printf "${RED}${golden_img_mode} is not a valid golden image mode${NC}\n"
    print_help
    exit 1
fi


test_modes=( "$@" )
test_modes_str=""
for mode in "${test_modes[@]}"; do
    test_modes_str="$test_modes_str \"$mode\""
done

echo "Build dir:   $DILIGENT_BUILD_DIR"
echo "Img mode:    $golden_img_mode"
echo "Img dir:     $golden_images_dir"
echo "Img size:    $GOLDEN_IMAGE_WIDTH x $GOLDEN_IMAGE_HEIGHT"
echo "Test modes:  $test_modes_str"
echo ""

declare -a TestApps=(
    "Tutorials/Tutorial01_HelloTriangle"
    "Tutorials/Tutorial02_Cube"
    "Tutorials/Tutorial03_Texturing"
    "Tutorials/Tutorial03_Texturing-C"
    "Tutorials/Tutorial04_Instancing"
    "Tutorials/Tutorial05_TextureArray"
    "Tutorials/Tutorial06_Multithreading"
    "Tutorials/Tutorial07_GeometryShader"
    "Tutorials/Tutorial08_Tessellation"
    "Tutorials/Tutorial09_Quads"
    "Tutorials/Tutorial10_DataStreaming"
    "Tutorials/Tutorial11_ResourceUpdates"
    "Tutorials/Tutorial12_RenderTarget"
    "Tutorials/Tutorial13_ShadowMap"
    "Tutorials/Tutorial14_ComputeShader"
    # "Tutorials/Tutorial16_BindlessResources" does not work properly on llvmpipe 
    "Tutorials/Tutorial17_MSAA"
    "Tutorials/Tutorial18_Queries --show_ui 0"
    "Tutorials/Tutorial19_RenderPasses"
    "Tutorials/Tutorial23_CommandQueues --show_ui 0"
    "Tutorials/Tutorial25_StatePackager --show_ui 0"
    "Tutorials/Tutorial26_StateCache --show_ui 0"
     # On the second run the states should be loaded from the cache
    "Tutorials/Tutorial26_StateCache --show_ui 0"
    "Samples/Atmosphere --show_ui 0"
    "Samples/GLTFViewer --show_ui 0"
    "Samples/NuklearDemo --show_ui 0"
    "Samples/Shadows --show_ui 0"
    # "Samples/ImguiDemo" has fps counter in the UI, so we have to skip it
)

tests_failed=0
tests_passed=0
tests_skipped=0
overall_status=""

function process_golden_img
{
    IFS='/ ' read -r -a inputs <<< "$1"

    local app_folder=${inputs[0]}
    local app_name=${inputs[1]}

    local extra_args=""
    for i in "${!inputs[@]}"; do
        if [[ "$i" -ge 2 ]]; then
            extra_args="$extra_args ${inputs[$i]}"
        fi
    done

    echo "Testing $app_folder/$app_name$extra_args"

    cd "$app_folder/$app_name/assets"

    local golden_img_dir=$golden_images_dir/$app_folder/$app_name
    if [ ! -d "$golden_img_dir" ]; then
        mkdir -p "$golden_img_dir"
    fi

    for mode in "${test_modes[@]}"; do
        local app_path=$DILIGENT_BUILD_DIR/DiligentSamples/$app_folder/$app_name/$app_name

        # Get the backend name
        IFS=' ' read -r -a args <<< "$mode"
        for i in "${!args[@]}"; do
            if [[ "${args[$i]}" == "-m" || "${args[$i]}" == "--mode" ]]; then
                let j=i+1
                backend_name="${args[$j]}"
            fi
        done

        skip_test=0
        if [[ "$backend_name" == "gl" && ("$app_name" == "Tutorial07_GeometryShader" || "$app_name" == "Tutorial08_Tessellation") ]]; then
            for i in "${!args[@]}"; do
                if [[ "${args[$i]}" == "--non_separable_progs" ]]; then
                    let j=i+1
                    if [[ "${args[$j]}" != "0" ]]; then
                        skip_test=1
                    fi
                fi
            done
        fi

        if [[ "$skip_test" == "0" ]]; then
            local capture_name="$app_name""_""$backend_name"

            local cmd="$app_path $mode --width $GOLDEN_IMAGE_WIDTH --height $GOLDEN_IMAGE_HEIGHT --golden_image_mode $golden_img_mode --capture_path $golden_img_dir --capture_name $capture_name --capture_format png --adapters_dialog 0 $extra_args"
            echo $cmd
            echo ""
            bash -c "$cmd"
            local res=$?

            # Note that linux return codes are always positive, and we only get the low 8 bits
            local status=""
            if [[ "$res" == "0" ]]; then
                let tests_passed=$tests_passed+1

                if [[ "$golden_img_mode" == "compare" ]];        then status="${GREEN}Golden image validation PASSED for $app_name ($mode).${NC}"; fi
                if [[ "$golden_img_mode" == "capture" ]];        then status="${GREEN}Successfully generated golden image for $app_name ($mode).${NC}"; fi
                if [[ "$golden_img_mode" == "compare_update" ]]; then status="${GREEN}Golden image validation PASSED for $app_name ($mode). Image updated.${NC}"; fi
            else
                let tests_failed=$tests_failed+1

                if [[ "$golden_img_mode" == "compare" ]];        then status="${RED}Golden image validation FAILED for $app_name ($mode). Error code: $res.${NC}"; fi
                if [[ "$golden_img_mode" == "capture" ]];        then status="${RED}FAILED to generate golden image for $app_name ($mode). Error code: $res.${NC}"; fi
                if [[ "$golden_img_mode" == "compare_update" ]]; then status="${RED}Golden image validation FAILED for $app_name ($mode). Error code: $res.${NC}"; fi
            fi
        else
            status="${YELLOW}Golden image processing SKIPPED for $app_name ($mode).${NC}"
            let tests_skipped=$tests_skipped+1
        fi

        overall_status="$overall_status$status\n"

        printf "$status\n\n\n"  
    done

    cd ../../../
}

cd ..

# NB: we must iterate using the index because
#       for App in ${TestApps[@]}; do
#     splits the strings at spaces
for i in "${!TestApps[@]}"; do
    process_golden_img "${TestApps[$i]}"
done

cd Tests

printf "$overall_status\n"

if [[ "$tests_passed" != "0" ]]; then
    printf "${GREEN}$tests_passed tests PASSED${NC}\n"
fi

if [[ "$tests_failed" != "0" ]]; then
    printf "${RED}$tests_failed tests FAILED${NC}\n"
fi

if [[ "$tests_skipped" != "0" ]]; then
    printf "${YELLOW}$tests_skipped tests SKIPPED${NC}\n"
fi

exit $tests_failed
