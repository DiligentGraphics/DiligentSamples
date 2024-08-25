import os
import shutil

from argparse import ArgumentParser

BUILD_TARGETS = [
    "Tutorial01_HelloTriangle",
    "Tutorial02_Cube",
    "Tutorial03_Texturing",
    "Tutorial04_Instancing",
    "Tutorial05_TextureArray",
    "Tutorial06_Multithreading",
    "Tutorial09_Quads",
    "Tutorial10_DataStreaming",
    "Tutorial11_ResourceUpdates",
    "Tutorial12_RenderTarget",
    "Tutorial13_ShadowMap",
    "Tutorial14_ComputeShader",
    "Tutorial16_BindlessResources",
    "Tutorial17_MSAA",
    "Tutorial18_Queries",
    "Tutorial19_RenderPasses",
    "Tutorial26_StateCache",
    "Tutorial27_PostProcessing",

    "Atmosphere",
    "ImguiDemo",
    "Shadows",
    "GLTFViewer"
]

def pack_artifacts(source_dir, destination_dir):
    if not os.path.exists(destination_dir):
        os.makedirs(destination_dir)

    for folder in BUILD_TARGETS:
        source_folder_path_tutorials = os.path.join(source_dir, "Tutorials", folder)
        source_folder_path_samples = os.path.join(source_dir, "Samples", folder)
        destination_folder_path = os.path.join(destination_dir, folder)
       
        if os.path.exists(source_folder_path_tutorials):
            shutil.copytree(source_folder_path_tutorials, destination_folder_path, dirs_exist_ok=True)
        elif os.path.exists(source_folder_path_samples):
            shutil.copytree(source_folder_path_samples, destination_folder_path, dirs_exist_ok=True)
        else:
            print(f"Folder '{folder}' not found.")

if __name__ == '__main__':
    parser = ArgumentParser("Pack WASM modules")
    parser.add_argument("-s", "--source_dir",
                        required=True)

    parser.add_argument("-o", "--output_dir",
                        required=True)

    args = parser.parse_args()
    pack_artifacts(args.source_dir, args.output_dir)
