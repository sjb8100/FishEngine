#include <iostream>
#include <map>
#include <sstream>

#include "FishEditorWindow.hpp"

using namespace FishEditor;

#include "GameObject.hpp"
#include "RenderSystem.hpp"
#include "Debug.hpp"

#include "App.hpp"
#include "Input.hpp"
#include "EditorGUI.hpp"
#include "Camera.hpp"
#include "Time.hpp"
#include "Mesh.hpp"
#include "MeshFilter.hpp"
#include "MeshRenderer.hpp"
#include "RenderSettings.hpp"
#include "Scene.hpp"
#include "Selection.hpp"
#include "EditorRenderSystem.hpp"
#include "Light.hpp"
#include <ModelImporter.hpp>
#include <CameraController.hpp>

using namespace std;
using namespace FishEngine;
using namespace FishEditor;

class ShowFPS : public Script
{
public:
    InjectClassName(ShowFPS);

    int m_fps = 0;
    
    virtual void OnInspectorGUI() override {
        m_fps = (int)floor(1.f / Time::deltaTime() + 0.5f);
        ImGui::Text("FPS: %d", m_fps);
    }

    virtual void Update() override {
        if (Input::GetKeyDown(KeyCode::A)) {
            Debug::Log("A pressed");
        }
        if (Input::GetKey(KeyCode::A)) {
            Debug::Log("A held");
        }
        if (Input::GetKeyUp(KeyCode::A)) {
            Debug::Log("A released");
        }
    }
};

class DeactiveSelf : public Script
{
public:
    InjectClassName(DeactiveSelf);

    bool m_active = true;
    
    virtual void OnInspectorGUI() override {
        ImGui::Checkbox("show", &m_active);
    }

    virtual void Update() override {
        if (m_active && !gameObject()->activeSelf()) {
            Debug::Log("show");
            gameObject()->SetActive(true);
        }
        if (!m_active &&  gameObject()->activeSelf()) {
            Debug::Log("hide");
            gameObject()->SetActive(false);
        }
    }
};

class VisualizeNormal : public Script
{
private:
    bool m_added = false;
    shared_ptr<MeshRenderer> m_meshRenderer = nullptr;
    Material::PMaterial m_material = nullptr;

public:
    InjectClassName(VisualizeNormal);

    bool m_visualizeNormal = false;

    virtual void Start() override {
        m_meshRenderer = gameObject()->GetComponent<MeshRenderer>();
        m_material = Material::builtinMaterial("VisualizeNormal");
    }
    
    virtual void OnInspectorGUI() override {
        ImGui::Checkbox("Visualize Normal##checkbox", &m_visualizeNormal);
    }
    
    virtual void Update() override {
        auto& materials = m_meshRenderer->materials();
        if (m_visualizeNormal) {
            if (!m_added) {
                m_meshRenderer->AddMaterial(m_material);
                m_added = true;
            }
        } else {
            if (materials[materials.size()-1] == m_material) {
                materials.pop_back();
                m_added = false;
            }
        }
    }
};

class TakeScreenShot : public Script
{
public:
    InjectClassName(TakeScreenShot);
    
    virtual void OnInspectorGUI() override {
        if (EditorGUI::Button("Screen shot")) {
            auto tm = time(nullptr);
            ostringstream ss;
            ss << "./" << int(tm) << ".png";
            EditorRenderSystem::SaveScreenShot(ss.str());
            Debug::Log("Screen shot saved to %s", ss.str().c_str());
        }
    }
};
            
class DisplayMatrix : public Script {
public:
    InjectClassName(DisplayMatrix);
    
    Matrix4x4 localToWorld;
    Matrix4x4 worldToLocal;
    // Use this for initialization
    
    virtual void OnInspectorGUI() override {
        EditorGUI::Matrix4x4("localToWorld", localToWorld);
        EditorGUI::Matrix4x4("worldToLocal", worldToLocal);
    }
    
    virtual void Start () override {
        localToWorld = transform()->localToWorldMatrix();
        worldToLocal = transform()->worldToLocalMatrix();
    }
};

class EditorRenderSettings : public Script
{
public:
    InjectClassName(EditorRenderSettings);

    bool m_isWireFrameMode = false;
    bool m_useGammaCorrection = true;
    bool m_showShadowMap = false;
    bool m_highlightSelections = false;
    
    virtual void Start() override {
        EditorRenderSystem::setWireFrameMode(m_isWireFrameMode);
        EditorRenderSystem::setGammaCorrection(m_useGammaCorrection);
        EditorRenderSystem::setShowShadowMap(m_showShadowMap);
        EditorRenderSystem::setHightlightSelections(m_highlightSelections);
    }

    virtual void OnInspectorGUI() override {
        if (ImGui::Checkbox("Wire Frame", &m_isWireFrameMode)) {
            EditorRenderSystem::setWireFrameMode(m_isWireFrameMode);
        }
        if (ImGui::Checkbox("Gamma Correction", &m_useGammaCorrection)) {
            EditorRenderSystem::setGammaCorrection(m_useGammaCorrection);
        }
        if (ImGui::Checkbox("Show ShadowMap", &m_showShadowMap)) {
            EditorRenderSystem::setShowShadowMap(m_showShadowMap);
        }
        if (ImGui::Checkbox("Hightlight Selections", &m_highlightSelections)) {
            EditorRenderSystem::setHightlightSelections(m_highlightSelections);
        }
    }
};
            
class Rotator : public Script {
public:
    InjectClassName(Rotator);
    
    bool rotating = false;
    float step = 1;
    
    virtual void Update() override {
        if (rotating)
            transform()->RotateAround(Vector3(0, 0, 0), Vector3::up, step);
    }
    
    virtual void OnInspectorGUI() override {
        ImGui::Checkbox("Rotating", &rotating);
    }
};


class TestPBR : public App
{
public:

    virtual void Init() override {
        glCheckError();
#if FISHENGINE_PLATFORM_WINDOWS
        const std::string root_dir = "../../assets/";
#else
        const std::string root_dir = "/Users/yushroom/program/graphics/FishEngine/assets/";
#endif
        const std::string models_dir = root_dir + "models/";
        const std::string textures_dir = root_dir + "textures/";

        auto sphere = Mesh::builtinMesh("sphere");
        auto cone = Mesh::builtinMesh("cone");
        auto cube = Mesh::builtinMesh("cube");
        auto plane = Mesh::builtinMesh("plane");
        
        //auto mitsuba = Mesh::CreateFromObjFile(models_dir + "mitsuba-sphere.obj");
        auto mitsuba = ModelImporter::LoadFromFile(models_dir + "mitsuba-sphere.obj");
        mitsuba->CreateGameObject();
        //auto boblampclean = Mesh::CreateFromObjFile(models_dir + "boblampclean.md5mesh");

        auto sky_texture = Texture::CreateFromFile(textures_dir + "StPeters/DiffuseMap.dds");
        //auto sky_texture = Texture::CreateFromFile(textures_dir + "uffizi_cross_filtered_y.dds");
        auto checkboard_texture = Texture::CreateFromFile(textures_dir + "checkboard.png");
        //auto head_diffuse = Texture::CreateFromFile(models_dir + "head/lambertian.jpg");
        //auto head_normalmap = Texture::CreateFromFile(models_dir + "head/NormalMap_RG16f_1024_mipmaps.dds");
        
        map<string, Texture::PTexture> textures;
        textures["skyTex"] = sky_texture;
        
        auto skyboxGO = Scene::CreateGameObject("SkyBox");
        skyboxGO->transform()->setLocalScale(20, 20, 20);
        auto meshFilter = make_shared<MeshFilter>(sphere);
        auto material = Material::builtinMaterial("SkyBox");
        material->BindTextures(textures);
        auto meshRenderer = make_shared<MeshRenderer>(material);
        skyboxGO->AddComponent(meshFilter);
        skyboxGO->AddComponent(meshRenderer);
        
        //textures.clear();
        //textures["diffuseMap"] = head_diffuse;
        //textures["normalMap"] = head_normalmap;

//        auto headGO = Scene::CreateGameObject();
//        headGO->transform()->setScale(10, 10, 10);
//        auto meshFilter1 = make_shared<MeshFilter>(headModel);
//        auto material1 = Material::builtinMaterial("NormalMap");
//        material1->BindTextures(textures);
//        auto meshRenderer1 = make_shared<MeshRenderer>(material1);
//        headGO->AddComponent(meshFilter1);
//        headGO->AddComponent(meshRenderer1);
//        headGO->AddScript(make_shared<VisualizeNormal>());
//        //headGO->AddScript(make_shared<DeactiveSelf>());
        textures["AmbientCubemap"] = sky_texture;
        
//        auto go = Scene::CreateGameObject("Sphere");
//        //go->transform()->setScale(20, 20, 20);
//        meshFilter = make_shared<MeshFilter>(mitsuba);
//        material = Material::builtinMaterial("PBR");
//        material->SetVector3("albedo", Vector3(0.972f, 0.960f, 0.915f));
//        material->BindTextures(textures);
//        meshRenderer = make_shared<MeshRenderer>(material);
//        go->AddComponent(meshFilter);
//        go->AddComponent(meshRenderer);
//        go->AddScript(make_shared<VisualizeNormal>());
        //go->AddScript(make_shared<DisplayMatrix>());
        //go->AddScript(make_shared<DeactiveSelf>());
        //go->SetActive(false);
        //Scene::SelectGameObject(go.get());

        
        auto group = Scene::CreateGameObject("Group");
        
        auto create_sphere = [&sphere, &textures, &group](
                                                //std::shared_ptr<GameObject>& parent,
                                                int x, int y) {
            auto go = Scene::CreateGameObject("Sphere");
            go->transform()->SetParent(group->transform());
            go->transform()->setLocalPosition(x*1.2f, y*1.2f, 0);
            //go->transform()->setPosition(0, 0, 2);
            go->transform()->setLocalEulerAngles(0, 30, 0);
            go->transform()->setLocalScale(0.5f, 0.5f, 0.5f);
            auto meshFilter = make_shared<MeshFilter>(sphere);
            auto material = Material::builtinMaterial("PBR");
            material->BindTextures(textures);
            material->SetFloat("metallic", 0.1f*x);
            material->SetFloat("roughness", 0.1f*y);
            material->SetVector3("albedo", Vector3(1.f, 0.6f, 0.6f));
            auto meshRenderer = make_shared<MeshRenderer>(material);
            go->AddComponent(meshFilter);
            go->AddComponent(meshRenderer);
            //go->AddScript(make_shared<DisplayMatrix>());
            //go->transform()->SetParent(parent->transform());
            return go;
        };

        
        for (int x = 0; x < 11; ++x) {
            for (int y = 0; y < 11; y++) {
                create_sphere(x, y);
            }
        }
        //create_sphere(0, 0);

//        auto child0 = create_cube(go);
//        auto child1 = create_cube(child0);
//        auto child2 = create_cube(child1);

//        go = Scene::CreateGameObject("Plane");
//        meshFilter = make_shared<MeshFilter>(plane);
//        material = Material::builtinMaterial("Diffuse");
//        textures.clear();
//        textures["diffuseMap"] = checkboard_texture;
//        material->BindTextures(textures);
//        meshRenderer = make_shared<MeshRenderer>(material);
//        go->AddComponent(meshFilter);
//        go->AddComponent(meshRenderer);
        //go->AddScript(make_shared<VisualizeNormal>());
        //go->AddScript(make_shared<DisplayMatrix>());
        
        auto cameraGO = Scene::mainCamera()->gameObject();
        cameraGO->transform()->setPosition(6, 6, -12);
        cameraGO->transform()->LookAt(6, 6, 0);
        //cameraGO->transform()->setPosition(0, 0, -5);
        //cameraGO->transform()->LookAt(0, 0, 0);
        //cameraGO->transform()->LookAt(0, 0, 0);
        cameraGO->AddScript(make_shared<ShowFPS>());
        cameraGO->AddScript(make_shared<TakeScreenShot>());
        //cameraGO->AddScript(make_shared<RenderSettings>());
        //cameraGO->AddScript(make_shared<DisplayMatrix>());
        cameraGO->AddScript(make_shared<EditorRenderSettings>());
        Selection::setActiveGameObject(cameraGO);

        auto go = Scene::CreateGameObject("Directional Light");
        go->transform()->setPosition(6, 5, -10);
        go->transform()->LookAt(6, 0, 0);
        //go->transform()->setPosition(6, 5, -10);
        //go->transform()->LookAt(0, 0, 0);
        go->AddComponent(Light::Create());
        go->AddScript(make_shared<Rotator>());
        
        
        //auto child0 = Scene::CreateGameObject("child0");
        //child0->transform()->SetParent(go->transform());
        //auto child1 = Scene::CreateGameObject("child1");
        //child1->transform()->SetParent(go->transform());
        //auto child3 = Scene::CreateGameObject("child3");
        //child3->transform()->SetParent(child0->transform());
    }
};

            
class TestAnimation : public App
{
public:

    virtual void Init() override {
        glCheckError();
#if FISHENGINE_PLATFORM_WINDOWS
        const std::string root_dir = "../../assets/";
#else
        const std::string root_dir = "/Users/yushroom/program/graphics/FishEngine/assets/";
#endif
        const std::string models_dir = root_dir + "models/";
        const std::string textures_dir = root_dir + "textures/";
        const std::string chan_dir = models_dir + "UnityChan/";
        
        auto sphere = Mesh::builtinMesh("sphere");
        auto cone = Mesh::builtinMesh("cone");
        auto cube = Mesh::builtinMesh("cube");
        auto plane = Mesh::builtinMesh("plane");
        
        //auto mitsuba = Mesh::CreateFromObjFile(models_dir + "mitsuba-sphere.obj");
        auto model = ModelImporter::LoadFromFile(chan_dir + "unitychan.fbx");
        //auto boblampclean = ModelImporter::LoadFromFile(models_dir + "TANGS_zou.FBX");
        //auto boblampclean = ModelImporter::LoadFromFile(models_dir+"Archer_max/archer_attacking.max");
        
        //auto sky_texture = Texture::CreateFromFile(textures_dir + "StPeters/DiffuseMap.dds");
        //auto checkboard_texture = Texture::CreateFromFile(textures_dir + "checkboard.png");
        //auto bodyTexture = Texture::CreateFromFile(chan_dir + "body_01.tga");
        //auto skinTexture = Texture::CreateFromFile(chan_dir + "skin_01.tga");
        //auto hairTexture = Texture::CreateFromFile(chan_dir + "hair_01.tga");
        //auto faceTexture = Texture::CreateFromFile(chan_dir + "face_00.tga");
        //auto eyelineTexture = Texture::CreateFromFile(chan_dir + "eyeline_00.tga");
        //auto eyeirisLTexture = Texture::CreateFromFile(chan_dir + "eye_iris_L_00.tga");
        //auto eyeirisRTexture = Texture::CreateFromFile(chan_dir + "eye_iris_R_00.tga");
        //auto cheekTexture = Texture::CreateFromFile(chan_dir + "cheek_00.tga");

        //map<string, Texture::PTexture> textures;
        //textures["skyTex"] = sky_texture;
        //
        //auto skyboxGO = Scene::CreateGameObject("SkyBox");
        //skyboxGO->transform()->setLocalScale(20, 20, 20);
        //auto meshFilter = make_shared<MeshFilter>(sphere);
        //auto material = Material::builtinMaterial("SkyBox");
        //material->BindTextures(textures);
        //auto meshRenderer = make_shared<MeshRenderer>(material);
        //skyboxGO->AddComponent(meshFilter);
        //skyboxGO->AddComponent(meshRenderer);

        //textures["AmbientCubemap"] = sky_texture;
        //
        //auto go = model->CreateGameObject();
        ////go->transform()->setLocalEulerAngles(90, 0, 0);
        //go->transform()->setLocalScale(0.05f, 0.05f, 0.05f);
        //go->transform()->setLocalPosition(0, -4, 0);

        //material = Material::builtinMaterial("TextureDoubleSided");
        //textures["DiffuseMap"] = bodyTexture;
        //auto material2 = Material::builtinMaterial("Outline");
        //material2->SetVector4("_Color", Vector4(1, 1, 1, 1));
        //material2->SetTexture("_MainTex", skinTexture);
        //material2->SetFloat("_EdgeThickness", 4.0f);
        //material->BindTextures(textures);
        //for (auto idx : {12, 14, 15, 16, 17, 18, 19, 20, 22}) {
        //    auto child = go->transform()->GetChild(idx)->gameObject();
        //    child->GetComponent<MeshRenderer>()->SetMaterial(material);
        //    child->GetComponent<MeshRenderer>()->AddMaterial(material2);
        //}

        //textures["DiffuseMap"] = skinTexture;
        //material = Material::builtinMaterial("Texture");
        //material->BindTextures(textures);

        //material2 = Material::builtinMaterial("Outline");
        //material2->SetVector4("_Color", Vector4(1, 1, 1, 1));
        //material2->SetTexture("_MainTex", skinTexture);
        //material2->SetFloat("_EdgeThickness", 4.0f);

        //for (auto idx : {0, 13}) {
        //    auto child = go->transform()->GetChild(idx)->gameObject();
        //    material = Material::builtinMaterial("Texture");
        //    material->SetTexture("DiffuseMap", skinTexture);
        //    child->GetComponent<MeshRenderer>()->SetMaterial(material);
        //    child->GetComponent<MeshRenderer>()->AddMaterial(material2);
        //}

        //material2 = Material::builtinMaterial("Outline");
        //material2->SetVector4("_Color", Vector4(1, 1, 1, 1));
        //material2->SetTexture("_MainTex", faceTexture);
        //material2->SetFloat("_EdgeThickness", 4.0f);
        //textures["DiffuseMap"] = faceTexture;
        //material = Material::builtinMaterial("Texture");
        //material->BindTextures(textures);
        //for (auto idx : { 4, 5 }) {
        //    auto child = go->transform()->GetChild(idx)->gameObject();
        //    child->GetComponent<MeshRenderer>()->SetMaterial(material);
        //    child->GetComponent<MeshRenderer>()->AddMaterial(material2);
        //}

        //material2 = Material::builtinMaterial("Outline");
        //material2->SetVector4("_Color", Vector4(1, 1, 1, 1));
        //material2->SetTexture("_MainTex", hairTexture);
        //material2->SetFloat("_EdgeThickness", 4.0f);
        //textures["DiffuseMap"] = hairTexture;
        //material = Material::builtinMaterial("TextureDoubleSided");
        //material->BindTextures(textures);
        //for (auto idx : { 8, 9, 10, 11}) {
        //    auto child = go->transform()->GetChild(idx)->gameObject();
        //    child->GetComponent<MeshRenderer>()->SetMaterial(material);
        //    child->GetComponent<MeshRenderer>()->AddMaterial(material2);
        //}

        //textures["DiffuseMap"] = eyeirisLTexture;
        //material = Material::builtinMaterial("Transparent");
        //material->BindTextures(textures);
        //for (auto idx : { 2 }) {
        //    auto child = go->transform()->GetChild(idx)->gameObject();
        //    child->GetComponent<MeshRenderer>()->SetMaterial(material);
        //}

        //textures["DiffuseMap"] = eyeirisRTexture;
        //material = Material::builtinMaterial("Transparent");
        //material->BindTextures(textures);
        //for (auto idx : { 3 }) {
        //    auto child = go->transform()->GetChild(idx)->gameObject();
        //    child->GetComponent<MeshRenderer>()->SetMaterial(material);
        //}

        //textures["DiffuseMap"] = eyelineTexture;
        //material = Material::builtinMaterial("Transparent");
        //material->BindTextures(textures);
        //for (auto idx : {1, 6, 7 }) {
        //    auto child = go->transform()->GetChild(idx)->gameObject();
        //    child->GetComponent<MeshRenderer>()->SetMaterial(material);
        //}

        //textures["DiffuseMap"] = cheekTexture;
        //material = Material::builtinMaterial("Transparent");
        //material->BindTextures(textures);
        //for (auto idx : { 21 }) {
        //    auto child = go->transform()->GetChild(idx)->gameObject();
        //    child->GetComponent<MeshRenderer>()->SetMaterial(material);
        //}
        
        auto cameraGO = Scene::mainCamera()->gameObject();
        cameraGO->transform()->setPosition(0, 0, -7);
        cameraGO->transform()->LookAt(0, 0, 0);
        //cameraGO->transform()->setPosition(0, 0, -5);
        //cameraGO->transform()->LookAt(0, 0, 0);
        //cameraGO->transform()->LookAt(0, 0, 0);
        cameraGO->AddScript(make_shared<ShowFPS>());
        cameraGO->AddScript(make_shared<TakeScreenShot>());
        //cameraGO->AddScript(make_shared<RenderSettings>());
        //cameraGO->AddScript(make_shared<DisplayMatrix>());
        auto s = make_shared<EditorRenderSettings>();
        s->m_useGammaCorrection = false;
        cameraGO->AddScript(s);
        //cameraGO->GetComponent<EditorRenderSettings>()->m_useGammaCorrection = false;

        Selection::setActiveGameObject(cameraGO);
        
        auto go = Scene::CreateGameObject("Directional Light");
        go->transform()->setPosition(6, 5, -10);
        go->transform()->LookAt(0, 0, 0);
        //go->transform()->setPosition(6, 5, -10);
        //go->transform()->LookAt(0, 0, 0);
        go->AddComponent(Light::Create());
        go->AddScript(make_shared<Rotator>());
    }
};

int main()
{
    FishEditorWindow::AddApp(make_shared<TestAnimation>());
    FishEditorWindow::Init();
    FishEditorWindow::Run();
    FishEditorWindow::Clean();
    return 0;
}