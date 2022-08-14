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

if [[ $# -lt 3 ]]; then   
    echo "At least three arguments are required"
    print_help
    exit -1
fi


if [[ "$DILIGENT_BUILD_DIR" == "" ]]; then
    echo "Required DILIGENT_BUILD_DIR variable is not set"
    print_help
    exit -1
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

declare -a Tutorials=(
    "Tutorial01_HelloTriangle"
    "Tutorial02_Cube"
    "Tutorial03_Texturing"
    "Tutorial03_Texturing-C"
    "Tutorial04_Instancing"
    "Tutorial05_TextureArray"
    "Tutorial06_Multithreading"
    "Tutorial07_GeometryShader"
    "Tutorial08_Tessellation"
    "Tutorial09_Quads"
    "Tutorial10_DataStreaming"
    "Tutorial11_ResourceUpdates"
    "Tutorial12_RenderTarget"
    "Tutorial13_ShadowMap"
    "Tutorial14_ComputeShader"
    # "Tutorial16_BindlessResources" does not work properly on llvmpipe 
    "Tutorial17_MSAA"
    "Tutorial18_Queries"
    "Tutorial19_RenderPasses"
    "Tutorial23_CommandQueues"
)

declare -a Samples=(
    "Atmosphere"
    "GLTFViewer"
    "NuklearDemo"
    "Shadows"
    # "ImguiDemo" has fps counter in the UI, so we have to skip it
)

tests_failed=0
tests_passed=0

function process_golden_img
{
    local app_folder=$1
    local app_name=$2
        
    echo "Testing $app_folder/$app_name..."
    echo  ""

    local show_ui=1
    if [[ "$app_folder" == "Samples" || "$app_name" == "Tutorial18_Queries" || "$app_name" == "Tutorial23_CommandQueues" ]]; then
        show_ui=0
    fi

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

        local capture_name="$app_name""_gi_""$backend_name"

        local cmd="$app_path $mode --width $GOLDEN_IMAGE_WIDTH --height $GOLDEN_IMAGE_HEIGHT --golden_image_mode $golden_img_mode --capture_path $golden_img_dir --capture_name $capture_name --capture_format png --adapters_dialog 0 --show_ui $show_ui"
        echo $cmd
        echo ""
        bash -c "$cmd"

        local res=$?
        if [[ "$res" == "0" ]]; then
            let tests_passed=$tests_passed+1
        else
            let tests_failed=$tests_failed+1
        fi

        if [[ "$golden_img_mode" == "compare" ]]; then
            if [[ "$res" -eq "0" ]]; then echo "Golden image validation PASSED for $app_name ($mode)."; fi
            if [[ "$res" -gt "0" ]]; then echo "Golden image validation FAILED for $app_name ($mode): $res inconsistent pixels found."; fi
            if [[ "$res" -lt "0" ]]; then echo "Golden image validation FAILED for $app_name ($mode) with error $res."; fi
        fi
        if [[ "$golden_img_mode" == "capture" ]]; then
            if [[ "$res" -eq "0" ]]; then echo "Successfully generated golden image for $app_name ($mode)."; fi
            if [[ "$res" -gt "0" ]]; then echo "FAILED to generate golden image for $app_name ($mode). Error code: $res."; fi
            if [[ "$res" -lt "0" ]]; then echo "FAILED to generate golden image for $app_name ($mode). Error code: $res."; fi        
        fi
        if [[ "$golden_img_mode" == "compare_update" ]]; then
            if [[ "$res" -eq "0" ]]; then echo "Golden image validation PASSED for $app_name ($mode). Image updated."; fi
            if [[ "$res" -gt "0" ]]; then echo "Golden image validation FAILED for $app_name ($mode): $res inconsistent pixels found. Image updated."; fi
            if [[ "$res" -lt "0" ]]; then echo "Golden image validation FAILED for $app_name ($mode) with error $res."; fi
        fi
        
		echo ""
        echo ""
    done

    cd ../../../
}

cd ..

for App in ${Tutorials[@]}; do
   process_golden_img Tutorials $App $AppModes
done

for App in ${Samples[@]}; do
   process_golden_img Samples $App $AppModes
done

cd Tests

echo "$tests_passed tests PASSED"

if [[ "$tests_failed" != "0" ]]; then
    echo "$tests_failed tests FAILED"
fi

exit $tests_failed
