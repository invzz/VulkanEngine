import os
import re

# Map short names to full paths
short_mapping = {
    # Core
    "Window.hpp": "Engine/Core/Window.hpp",
    "Keyboard.hpp": "Engine/Core/Keyboard.hpp",
    "Mouse.hpp": "Engine/Core/Mouse.hpp",
    "Exceptions.hpp": "Engine/Core/Exceptions.hpp",
    "utils.hpp": "Engine/Core/utils.hpp",
    "ansi_colors.hpp": "Engine/Core/ansi_colors.hpp",

    # Graphics
    "Device.hpp": "Engine/Graphics/Device.hpp",
    "DeviceMemory.hpp": "Engine/Graphics/DeviceMemory.hpp",
    "SwapChain.hpp": "Engine/Graphics/SwapChain.hpp",
    "Pipeline.hpp": "Engine/Graphics/Pipeline.hpp",
    "Buffer.hpp": "Engine/Graphics/Buffer.hpp",
    "Descriptors.hpp": "Engine/Graphics/Descriptors.hpp",
    "Renderer.hpp": "Engine/Graphics/Renderer.hpp",
    "FrameInfo.hpp": "Engine/Graphics/FrameInfo.hpp",
    "RenderGraph.hpp": "Engine/Graphics/RenderGraph.hpp",
    "ShadowMap.hpp": "Engine/Graphics/ShadowMap.hpp",
    "CubeShadowMap.hpp": "Engine/Graphics/CubeShadowMap.hpp",
    "MorphTargetCompute.hpp": "Engine/Graphics/MorphTargetCompute.hpp",
    "ImGuiManager.hpp": "Engine/Graphics/ImGuiManager.hpp",

    # Scene
    "GameObject.hpp": "Engine/Scene/GameObject.hpp",
    "GameObjectManager.hpp": "Engine/Scene/GameObjectManager.hpp",
    "Camera.hpp": "Engine/Scene/Camera.hpp",
    "Skybox.hpp": "Engine/Scene/Skybox.hpp",
    "AnimationController.hpp": "Engine/Scene/AnimationController.hpp",
    "SceneSerializer.hpp": "Engine/Scene/SceneSerializer.hpp",
    
    # Resources
    "Model.hpp": "Engine/Resources/Model.hpp",
    "Texture.hpp": "Engine/Resources/Texture.hpp",
    "TextureManager.hpp": "Engine/Resources/TextureManager.hpp",
    "MeshManager.hpp": "Engine/Resources/MeshManager.hpp",
    "MorphTargetManager.hpp": "Engine/Resources/MorphTargetManager.hpp",
    "ResourceManager.hpp": "Engine/Resources/ResourceManager.hpp",
    "PBRMaterial.hpp": "Engine/Resources/PBRMaterial.hpp",

    # Systems
    "IBLSystem.hpp": "Engine/Systems/IBLSystem.hpp",
    "MeshRenderSystem.hpp": "Engine/Systems/MeshRenderSystem.hpp",
    "SkyboxRenderSystem.hpp": "Engine/Systems/SkyboxRenderSystem.hpp",
    "ShadowSystem.hpp": "Engine/Systems/ShadowSystem.hpp",
    "PointLightSystem.hpp": "Engine/Systems/PointLightSystem.hpp",
    "CameraSystem.hpp": "Engine/Systems/CameraSystem.hpp",
    "InputSystem.hpp": "Engine/Systems/InputSystem.hpp",
    "ObjectSelectionSystem.hpp": "Engine/Systems/ObjectSelectionSystem.hpp",
    "PostProcessingSystem.hpp": "Engine/Systems/PostProcessingSystem.hpp",
    "AnimationSystem.hpp": "Engine/Systems/AnimationSystem.hpp",
    "LODSystem.hpp": "Engine/Systems/LODSystem.hpp",
    "MaterialSystem.hpp": "Engine/Systems/MaterialSystem.hpp",
    "MorphTargetSystem.hpp": "Engine/Systems/MorphTargetSystem.hpp",
    "LightSystem.hpp": "Engine/Systems/LightSystem.hpp",
}

def process_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()
    
    original_content = content
    
    for short_name, full_path in short_mapping.items():
        # Regex to match #include "Window.hpp" but NOT #include "Engine/Core/Window.hpp"
        # We look for quotes or brackets, then the short name, then closing quote/bracket.
        # And we ensure it's not preceded by a directory separator.
        
        # Pattern: "Window.hpp" -> "Engine/Core/Window.hpp"
        # We use negative lookbehind to ensure no / before the name
        
        # Regex for "Window.hpp"
        pattern_quote = r'"(?<!/)' + re.escape(short_name) + r'"'
        replacement_quote = f'"{full_path}"'
        content = re.sub(pattern_quote, replacement_quote, content)
        
        # Regex for <Window.hpp>
        pattern_bracket = r'<(?<!/)' + re.escape(short_name) + r'>'
        replacement_bracket = f'<{full_path}>'
        content = re.sub(pattern_bracket, replacement_bracket, content)

        # Also handle "systems/Foo.hpp" -> "Engine/Systems/Foo.hpp"
        if short_name.endswith("System.hpp"):
             pattern_sys = r'"systems/' + re.escape(short_name) + r'"'
             replacement_sys = f'"{full_path}"'
             content = re.sub(pattern_sys, replacement_sys, content)
             
        # Handle "../Foo.hpp" -> "Engine/.../Foo.hpp"
        pattern_rel = r'"\.\./' + re.escape(short_name) + r'"'
        replacement_rel = f'"{full_path}"'
        content = re.sub(pattern_rel, replacement_rel, content)

    if content != original_content:
        print(f"Updating {filepath}")
        with open(filepath, 'w') as f:
            f.write(content)

def main():
    dirs_to_scan = ['src', 'include']
    for root_dir in dirs_to_scan:
        for root, dirs, files in os.walk(root_dir):
            for file in files:
                if file.endswith('.hpp') or file.endswith('.cpp') or file.endswith('.h') or file.endswith('.c'):
                    process_file(os.path.join(root, file))

if __name__ == "__main__":
    main()
