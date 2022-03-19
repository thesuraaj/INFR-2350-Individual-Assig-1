#include "RenderLayer.h"
#include "../Application.h"
#include "Graphics/GuiBatcher.h"
#include "Gameplay/Components/Camera.h"
#include "Graphics/DebugDraw.h"
#include "Graphics/Textures/TextureCube.h"
#include "../Timing.h"
#include "Gameplay/Components/ComponentManager.h"
#include "Gameplay/Components/RenderComponent.h"

// GLM math library
#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <GLM/gtx/common.hpp> // for fmod (floating modulus)


RenderLayer::RenderLayer() :
	ApplicationLayer(),
	_primaryFBO(nullptr),
	_blitFbo(true),
	_frameUniforms(nullptr),
	_instanceUniforms(nullptr),
	_renderFlags(RenderFlags::EnableColorCorrection),
	_clearColor({ 0.1f, 0.1f, 0.1f, 1.0f })
{
	Name = "Rendering";
	Overrides = AppLayerFunctions::OnAppLoad | AppLayerFunctions::OnRender | AppLayerFunctions::OnWindowResize;
}

RenderLayer::~RenderLayer() = default;

void RenderLayer::OnRender(const Framebuffer::Sptr& prevLayer)
{
	using namespace Gameplay;

	Application& app = Application::Get();

	glViewport(0, 0, _primaryFBO->GetWidth(), _primaryFBO->GetHeight());

	// We bind our framebuffer so we can render to it
	_primaryFBO->Bind();

	// Clear the color and depth buffers
	glClearColor(_clearColor.x, _clearColor.y, _clearColor.z, _clearColor.w);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Grab shorthands to the camera and shader from the scene
	Camera::Sptr camera = app.CurrentScene()->MainCamera;

	// Cache the camera's viewprojection
	glm::mat4 viewProj = camera->GetViewProjection();
	DebugDrawer::Get().SetViewProjection(viewProj);

	// Make sure depth testing and culling are re-enabled
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	// The current material that is bound for rendering
	Material::Sptr currentMat = nullptr;
	ShaderProgram::Sptr shader = nullptr;

	// Bind the skybox texture to a reserved texture slot
	// See Material.h and Material.cpp for how we're reserving texture slots
	TextureCube::Sptr environment = app.CurrentScene()->GetSkyboxTexture();
	if (environment) {
		environment->Bind(15);
	}

	// Binding the color correction LUT
	Texture3D::Sptr colorLUT = app.CurrentScene()->GetColorLUT();
	if (colorLUT) {
		colorLUT->Bind(14);
	}

	// Here we'll bind all the UBOs to their corresponding slots
	app.CurrentScene()->PreRender();
	_frameUniforms->Bind(FRAME_UBO_BINDING);
	_instanceUniforms->Bind(INSTANCE_UBO_BINDING);

	// Draw physics debug
	app.CurrentScene()->DrawPhysicsDebug();

	// Upload frame level uniforms
	auto& frameData = _frameUniforms->GetData();
	frameData.u_Projection = camera->GetProjection();
	frameData.u_View = camera->GetView();
	frameData.u_ViewProjection = camera->GetViewProjection();
	frameData.u_CameraPos = glm::vec4(camera->GetGameObject()->GetPosition(), 1.0f);
	frameData.u_Time = static_cast<float>(Timing::Current().TimeSinceSceneLoad());
	frameData.u_DeltaTime = Timing::Current().DeltaTime();
	frameData.u_RenderFlags = _renderFlags;
	_frameUniforms->Update();

	Material::Sptr defaultMat = app.CurrentScene()->DefaultMaterial;

	// Render all our objects
	app.CurrentScene()->Components().Each<RenderComponent>([&](const RenderComponent::Sptr& renderable) {
		// Early bail if mesh not set
		if (renderable->GetMesh() == nullptr) {
			return;
		}

		// If we don't have a material, try getting the scene's fallback material
		// If none exists, do not draw anything
		if (renderable->GetMaterial() == nullptr) {
			if (defaultMat != nullptr) {
				renderable->SetMaterial(defaultMat);
			} else {
				return;
			}
		}

		// If the material has changed, we need to bind the new shader and set up our material and frame data
		// Note: This is a good reason why we should be sorting the render components in ComponentManager
		if (renderable->GetMaterial() != currentMat) {
			currentMat = renderable->GetMaterial();
			shader = currentMat->GetShader();

			shader->Bind();
			currentMat->Apply();
		}

		// Grab the game object so we can do some stuff with it
		GameObject* object = renderable->GetGameObject();

		// Use our uniform buffer for our instance level uniforms
		auto& instanceData = _instanceUniforms->GetData();
		instanceData.u_Model = object->GetTransform();
		instanceData.u_ModelViewProjection = viewProj * object->GetTransform();
		instanceData.u_NormalMatrix = glm::mat3(glm::transpose(glm::inverse(object->GetTransform())));
		_instanceUniforms->Update();

		// Draw the object
		renderable->GetMesh()->Draw();
	});

	// Use our cubemap to draw our skybox
	app.CurrentScene()->DrawSkybox();

	// Unbind our primary framebuffer so subsequent draw calls do not modify it
	//_primaryFBO->Unbind();

	VertexArrayObject::Unbind();
}

void RenderLayer::OnWindowResize(const glm::ivec2& oldSize, const glm::ivec2& newSize)
{
	if (newSize.x * newSize.y == 0) return;

	// Set viewport and resize our primary FBO
	_primaryFBO->Resize(newSize);

	// Update the main camera's projection
	Application& app = Application::Get();
	app.CurrentScene()->MainCamera->ResizeWindow(newSize.x, newSize.y);
}

void RenderLayer::OnAppLoad(const nlohmann::json& config)
{
	Application& app = Application::Get();

	// GL states, we'll enable depth testing and backface fulling
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	// Create a new descriptor for our FBO
	FramebufferDescriptor fboDescriptor;
	fboDescriptor.Width = app.GetWindowSize().x;
	fboDescriptor.Height = app.GetWindowSize().y;
	fboDescriptor.GenerateUnsampled = false;
	fboDescriptor.SampleCount = 1;

	// Add a depth and color attachment (same as default)
	fboDescriptor.RenderTargets[RenderTargetAttachment::DepthStencil] ={ true, RenderTargetType::DepthStencil };
	fboDescriptor.RenderTargets[RenderTargetAttachment::Color0] ={ true, RenderTargetType::ColorRgb8 };

	// Create the primary FBO
	_primaryFBO = std::make_shared<Framebuffer>(fboDescriptor);

	// Create our common uniform buffers
	_frameUniforms = std::make_shared<UniformBuffer<FrameLevelUniforms>>(BufferUsage::DynamicDraw);
	_instanceUniforms = std::make_shared<UniformBuffer<InstanceLevelUniforms>>(BufferUsage::DynamicDraw);
}

const Framebuffer::Sptr& RenderLayer::GetPrimaryFBO() const {
	return _primaryFBO;
}

bool RenderLayer::IsBlitEnabled() const {
	return _blitFbo;
}

void RenderLayer::SetBlitEnabled(bool value) {
	_blitFbo = value;
}

Framebuffer::Sptr RenderLayer::GetRenderOutput() {
	return _primaryFBO;
}

const glm::vec4& RenderLayer::GetClearColor() const {
	return _clearColor;
}

void RenderLayer::SetClearColor(const glm::vec4 & value) {
	_clearColor = value;
}

void RenderLayer::SetRenderFlags(RenderFlags value) {
	_renderFlags = value;
}

RenderFlags RenderLayer::GetRenderFlags() const {
	return _renderFlags;
}
