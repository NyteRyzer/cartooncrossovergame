#include "Render.h"

#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace
{
	const char* CelVS = R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 fragNormal;
out vec3 fragWorld;
void main()
{
    fragWorld = (matModel * vec4(vertexPosition, 1.0)).xyz;
    fragNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

	// Banded toon lighting with a stepped rim light and distance fog.
	const char* CelFS = R"(#version 330
in vec3 fragNormal;
in vec3 fragWorld;
uniform vec4 colDiffuse;
uniform vec3 lightDir;
uniform vec3 ambient;
uniform vec3 fogColor;
uniform float fogDensity;
uniform float bands;
uniform vec3 viewPos;
uniform float emissive;
out vec4 finalColor;
void main()
{
    vec3 n = normalize(fragNormal);
    if (!gl_FrontFacing) n = -n;
    float ndl = max(dot(n, -normalize(lightDir)), 0.0);
    float s = ndl * bands;
    float level = (floor(s) + smoothstep(0.42, 0.58, fract(s))) / bands;
    vec3 v = normalize(viewPos - fragWorld);
    float rim = smoothstep(0.62, 0.70, 1.0 - max(dot(n, v), 0.0)) * step(0.05, ndl);
    vec3 lit = colDiffuse.rgb * (ambient + vec3(1.0, 0.97, 0.9) * level * 0.85) + rim * 0.16;
    lit = mix(lit, colDiffuse.rgb * 1.2, emissive);
    float dist = length(viewPos - fragWorld);
    float fog = 1.0 - exp(-pow(dist * fogDensity, 2.0));
    finalColor = vec4(mix(lit, fogColor, clamp(fog, 0.0, 1.0) * (1.0 - emissive * 0.6)), colDiffuse.a);
}
)";

	const char* NormalFS = R"(#version 330
in vec3 fragNormal;
in vec3 fragWorld;
uniform float lineWeight;
out vec4 finalColor;
void main()
{
    vec3 n = normalize(fragNormal);
    if (!gl_FrontFacing) n = -n;
    finalColor = vec4(n * 0.5 + 0.5, lineWeight);
}
)";

	const char* SpriteVS = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec2 fragTexCoord;
out vec3 fragNormal;
out vec3 fragWorld;
void main()
{
    fragTexCoord = vertexTexCoord;
    fragWorld = (matModel * vec4(vertexPosition, 1.0)).xyz;
    fragNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

	// The art is already shaded, so sprites are unlit apart from fog and a
	// white hit-flash. Transparent pixels are discarded so depth (and so the
	// outline pass) follows the character's silhouette.
	const char* SpriteFS = R"(#version 330
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragWorld;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float flash;
uniform vec3 fogColor;
uniform float fogDensity;
uniform vec3 viewPos;
out vec4 finalColor;
void main()
{
    vec4 t = texture(texture0, fragTexCoord);
    if (t.a < 0.5) discard;
    vec3 c = mix(t.rgb * colDiffuse.rgb, vec3(1.0), flash);
    float dist = length(viewPos - fragWorld);
    float fog = 1.0 - exp(-pow(dist * fogDensity, 2.0));
    finalColor = vec4(mix(c, fogColor, clamp(fog, 0.0, 1.0)), colDiffuse.a);
}
)";

	const char* SpriteNormalFS = R"(#version 330
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragWorld;
uniform sampler2D texture0;
uniform float lineWeight;
out vec4 finalColor;
void main()
{
    if (texture(texture0, fragTexCoord).a < 0.5) discard;
    finalColor = vec4(normalize(fragNormal) * 0.5 + 0.5, lineWeight);
}
)";

	// Static batch: colour per vertex, emissive and line weight in texcoord2.
	const char* BatchVS = R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec4 vertexColor;
in vec2 vertexTexCoord2;
uniform mat4 mvp;
uniform mat4 matModel;
out vec3 fragNormal;
out vec3 fragWorld;
out vec4 fragColor;
out vec2 fragExtra;
void main()
{
    fragWorld = (matModel * vec4(vertexPosition, 1.0)).xyz;
    fragNormal = normalize(vertexNormal);
    fragColor = vertexColor;
    fragExtra = vertexTexCoord2;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

	const char* BatchFS = R"(#version 330
in vec3 fragNormal;
in vec3 fragWorld;
in vec4 fragColor;
in vec2 fragExtra;
uniform vec3 lightDir;
uniform vec3 ambient;
uniform vec3 fogColor;
uniform float fogDensity;
uniform float bands;
uniform vec3 viewPos;
out vec4 finalColor;
void main()
{
    vec3 n = normalize(fragNormal);
    if (!gl_FrontFacing) n = -n;
    float ndl = max(dot(n, -normalize(lightDir)), 0.0);
    float s = ndl * bands;
    float level = (floor(s) + smoothstep(0.42, 0.58, fract(s))) / bands;
    vec3 v = normalize(viewPos - fragWorld);
    float rim = smoothstep(0.62, 0.70, 1.0 - max(dot(n, v), 0.0)) * step(0.05, ndl);
    vec3 base = fragColor.rgb;
    float emissive = fragExtra.x;
    vec3 lit = base * (ambient + vec3(1.0, 0.97, 0.9) * level * 0.85) + rim * 0.12;
    lit = mix(lit, base * 1.2, emissive);
    float dist = length(viewPos - fragWorld);
    float fog = 1.0 - exp(-pow(dist * fogDensity, 2.0));
    finalColor = vec4(mix(lit, fogColor, clamp(fog, 0.0, 1.0) * (1.0 - emissive * 0.6)), 1.0);
}
)";

	const char* BatchNormalFS = R"(#version 330
in vec3 fragNormal;
in vec3 fragWorld;
in vec4 fragColor;
in vec2 fragExtra;
out vec4 finalColor;
void main()
{
    vec3 n = normalize(fragNormal);
    if (!gl_FrontFacing) n = -n;
    finalColor = vec4(n * 0.5 + 0.5, fragExtra.y);
}
)";

	// Sobel on linear depth (relative) + normals, thickness from per-object weight.
	const char* OutlineFS = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform sampler2D normalTex;
uniform sampler2D depthTex;
uniform vec2 texel;
uniform vec2 nearFar;
uniform vec3 skyTop;
uniform int debugView;
out vec4 finalColor;

float linearDepth(vec2 uv)
{
    float d = texture(depthTex, uv).r;
    float z = d * 2.0 - 1.0;
    return 2.0 * nearFar.x * nearFar.y / (nearFar.y + nearFar.x - z * (nearFar.y - nearFar.x));
}

void main()
{
    vec2 uv = fragTexCoord;
    vec4 color = texture(texture0, uv);

    float w = texture(normalTex, uv).a;
    w = max(w, texture(normalTex, uv + vec2(texel.x * 2.0, 0.0)).a);
    w = max(w, texture(normalTex, uv - vec2(texel.x * 2.0, 0.0)).a);
    w = max(w, texture(normalTex, uv + vec2(0.0, texel.y * 2.0)).a);
    w = max(w, texture(normalTex, uv - vec2(0.0, texel.y * 2.0)).a);
    float thickness = mix(1.0, 4.5, w);

    float dc = linearDepth(uv);
    vec3 nc = texture(normalTex, uv).rgb * 2.0 - 1.0;
    bool centerSky = texture(normalTex, uv).a <= 0.0;

    // Inverse depth is affine across a plane in screen space, so its Laplacian
    // is ~0 on flat surfaces (no grazing-angle noise) and spikes at silhouettes.
    vec2 offs[8] = vec2[](vec2(-1,-1), vec2(0,-1), vec2(1,-1), vec2(-1,0), vec2(1,0), vec2(-1,1), vec2(0,1), vec2(1,1));
    float ic = 1.0 / max(dc, 0.001);
    float lap = 0.0, ne = 0.0;
    for (int i = 0; i < 8; i++)
    {
        vec2 suv = uv + offs[i] * texel * thickness;
        float is = 1.0 / max(linearDepth(suv), 0.001);
        lap += (abs(offs[i].x) + abs(offs[i].y) > 1.5 ? 0.5 : 1.0) * (is - ic);
        vec4 ns = texture(normalTex, suv);
        if (ns.a > 0.0 && !centerSky)
            ne = max(ne, 1.0 - clamp(dot(ns.rgb * 2.0 - 1.0, nc), 0.0, 1.0));
        else if ((ns.a > 0.0) != !centerSky)
            ne = 1.0; // object against sky
    }
    float depthEdge = smoothstep(0.25, 0.6, abs(lap) / ic);
    float normalEdge = smoothstep(0.2, 0.45, ne);
    float fade = 1.0 - smoothstep(250.0, 1100.0, dc);
    float edge = max(depthEdge, normalEdge) * fade;
    finalColor = vec4(mix(color.rgb, color.rgb * 0.1, edge), 1.0);
    if (debugView == 1) finalColor = vec4(texture(normalTex, uv).rgb, 1.0);
    if (debugView == 2) finalColor = vec4(vec3(fract(dc / 20.0)), 1.0);
    if (debugView == 3) finalColor = vec4(vec3(1.0 - edge), 1.0);
}
)";

	Vector3 ColorToVec(Color C)
	{
		return Vector3{ C.r / 255.0f, C.g / 255.0f, C.b / 255.0f };
	}

	void RecenterY(Mesh& M)
	{
		for (int i = 0; i < M.vertexCount; ++i)
		{
			M.vertices[i * 3 + 1] -= 0.5f;
		}
		UpdateMeshBuffer(M, 0, M.vertices, M.vertexCount * 3 * static_cast<int>(sizeof(float)), 0);
	}

	// Four-sided cone has its corners on the axes; turn it 45 degrees and widen
	// it so a unit pyramid exactly covers a unit square (house roofs).
	void SquarePyramid(Mesh& M)
	{
		const float C = 0.70710678f, S = 0.70710678f, K = 1.41421356f;
		for (int i = 0; i < M.vertexCount; ++i)
		{
			float* V = &M.vertices[i * 3];
			const float X = V[0], Z = V[2];
			V[0] = (X * C - Z * S) * K;
			V[2] = (X * S + Z * C) * K;
			float* N = &M.normals[i * 3];
			const float Nx = N[0], Nz = N[2];
			N[0] = Nx * C - Nz * S;
			N[2] = Nx * S + Nz * C;
		}
		UpdateMeshBuffer(M, 0, M.vertices, M.vertexCount * 3 * static_cast<int>(sizeof(float)), 0);
		UpdateMeshBuffer(M, 2, M.normals, M.vertexCount * 3 * static_cast<int>(sizeof(float)), 0);
	}

	void Begin3DCustom(const Camera3D& Cam, int Width, int Height)
	{
		rlDrawRenderBatchActive();
		rlMatrixMode(RL_PROJECTION);
		rlPushMatrix();
		rlLoadIdentity();
		const double Aspect = static_cast<double>(Width) / static_cast<double>(Height > 0 ? Height : 1);
		const double Top = ToonRenderer::NearPlane * std::tan(Cam.fovy * 0.5 * DEG2RAD);
		const double Right = Top * Aspect;
		rlFrustum(-Right, Right, -Top, Top, ToonRenderer::NearPlane, ToonRenderer::FarPlane);
		rlMatrixMode(RL_MODELVIEW);
		rlLoadIdentity();
		const Matrix View = MatrixLookAt(Cam.position, Cam.target, Cam.up);
		rlMultMatrixf(MatrixToFloat(View));
		rlEnableDepthTest();
	}
}

Matrix MakeTransform(Vector3 Position, Vector3 Scale, float YawRad, float PitchRad, float RollRad)
{
	Matrix M = MatrixScale(Scale.x, Scale.y, Scale.z);
	M = MatrixMultiply(M, MatrixRotateX(RollRad));
	M = MatrixMultiply(M, MatrixRotateZ(PitchRad));
	M = MatrixMultiply(M, MatrixRotateY(YawRad));
	return MatrixMultiply(M, MatrixTranslate(Position.x, Position.y, Position.z));
}

RenderTexture2D ToonRenderer::CreateTarget(int Width, int Height)
{
	RenderTexture2D T{};
	T.id = rlLoadFramebuffer(Width, Height);
	rlEnableFramebuffer(T.id);
	T.texture.id = rlLoadTexture(nullptr, Width, Height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
	T.texture.width = Width;
	T.texture.height = Height;
	T.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
	T.texture.mipmaps = 1;
	T.depth.id = rlLoadTextureDepth(Width, Height, false);
	T.depth.width = Width;
	T.depth.height = Height;
	T.depth.format = 19; // DEPTH_COMPONENT_24BIT
	T.depth.mipmaps = 1;
	rlFramebufferAttach(T.id, T.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
	rlFramebufferAttach(T.id, T.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
	if (!rlFramebufferComplete(T.id))
	{
		TraceLog(LOG_WARNING, "Toon: framebuffer incomplete");
	}
	rlDisableFramebuffer();
	SetTextureFilter(T.texture, TEXTURE_FILTER_BILINEAR);
	return T;
}

void ToonRenderer::DestroyTarget(RenderTexture2D& Target)
{
	if (Target.id != 0)
	{
		rlUnloadTexture(Target.texture.id);
		rlUnloadTexture(Target.depth.id);
		rlUnloadFramebuffer(Target.id);
	}
	Target = RenderTexture2D{};
}

bool ToonRenderer::Init(int Width, int Height)
{
	Meshes[static_cast<int>(MeshKind::Cube)] = GenMeshCube(1.0f, 1.0f, 1.0f);
	Meshes[static_cast<int>(MeshKind::Sphere)] = GenMeshSphere(0.5f, 14, 18);
	Meshes[static_cast<int>(MeshKind::Cylinder)] = GenMeshCylinder(0.5f, 1.0f, 24);
	Meshes[static_cast<int>(MeshKind::Cone)] = GenMeshCone(0.5f, 1.0f, 20);
	RecenterY(Meshes[static_cast<int>(MeshKind::Cylinder)]);
	Meshes[static_cast<int>(MeshKind::Pyramid)] = GenMeshCone(0.5f, 1.0f, 4);
	RecenterY(Meshes[static_cast<int>(MeshKind::Cone)]);
	RecenterY(Meshes[static_cast<int>(MeshKind::Pyramid)]);
	SquarePyramid(Meshes[static_cast<int>(MeshKind::Pyramid)]);

	CelShader = LoadShaderFromMemory(CelVS, CelFS);
	NormalShader = LoadShaderFromMemory(CelVS, NormalFS);
	OutlineShader = LoadShaderFromMemory(nullptr, OutlineFS);
	if (CelShader.id == rlGetShaderIdDefault() || NormalShader.id == rlGetShaderIdDefault() || OutlineShader.id == rlGetShaderIdDefault())
	{
		TraceLog(LOG_ERROR, "Toon: shader compilation failed");
		return false;
	}

	LocLightDir = GetShaderLocation(CelShader, "lightDir");
	LocAmbient = GetShaderLocation(CelShader, "ambient");
	LocFogColor = GetShaderLocation(CelShader, "fogColor");
	LocFogDensity = GetShaderLocation(CelShader, "fogDensity");
	LocBands = GetShaderLocation(CelShader, "bands");
	LocViewPos = GetShaderLocation(CelShader, "viewPos");
	LocEmissive = GetShaderLocation(CelShader, "emissive");
	LocLineWeight = GetShaderLocation(NormalShader, "lineWeight");
	LocNormalTex = GetShaderLocation(OutlineShader, "normalTex");
	LocDepthTex = GetShaderLocation(OutlineShader, "depthTex");
	LocTexel = GetShaderLocation(OutlineShader, "texel");
	LocNearFar = GetShaderLocation(OutlineShader, "nearFar");
	LocSkyTop = GetShaderLocation(OutlineShader, "skyTop");

	SpriteShader = LoadShaderFromMemory(SpriteVS, SpriteFS);
	SpriteNormalShader = LoadShaderFromMemory(SpriteVS, SpriteNormalFS);
	if (SpriteShader.id == rlGetShaderIdDefault() || SpriteNormalShader.id == rlGetShaderIdDefault())
	{
		TraceLog(LOG_ERROR, "Toon: sprite shader compilation failed");
		return false;
	}
	LocSpriteFlash = GetShaderLocation(SpriteShader, "flash");
	LocSpriteFogColor = GetShaderLocation(SpriteShader, "fogColor");
	LocSpriteFogDensity = GetShaderLocation(SpriteShader, "fogDensity");
	LocSpriteViewPos = GetShaderLocation(SpriteShader, "viewPos");
	LocSpriteLineWeight = GetShaderLocation(SpriteNormalShader, "lineWeight");
	SpriteMaterial = LoadMaterialDefault();
	SpriteMaterial.shader = SpriteShader;
	SpriteNormalMaterial = LoadMaterialDefault();
	SpriteNormalMaterial.shader = SpriteNormalShader;

	// Unit quad, feet at the origin: x -0.5..0.5, y 0..1, facing +Z.
	SpriteQuad = Mesh{};
	SpriteQuad.vertexCount = 4;
	SpriteQuad.triangleCount = 2;
	SpriteQuad.vertices = static_cast<float*>(MemAlloc(4 * 3 * sizeof(float)));
	SpriteQuad.texcoords = static_cast<float*>(MemAlloc(4 * 2 * sizeof(float)));
	SpriteQuad.normals = static_cast<float*>(MemAlloc(4 * 3 * sizeof(float)));
	SpriteQuad.indices = static_cast<unsigned short*>(MemAlloc(6 * sizeof(unsigned short)));
	const float V[12] = { -0.5f, 0, 0, 0.5f, 0, 0, 0.5f, 1, 0, -0.5f, 1, 0 };
	const float T[8] = { 0, 1, 1, 1, 1, 0, 0, 0 };
	const unsigned short I[6] = { 0, 1, 2, 0, 2, 3 };
	for (int i = 0; i < 12; ++i) SpriteQuad.vertices[i] = V[i];
	for (int i = 0; i < 8; ++i) SpriteQuad.texcoords[i] = T[i];
	for (int i = 0; i < 4; ++i) { SpriteQuad.normals[i * 3] = 0; SpriteQuad.normals[i * 3 + 1] = 0; SpriteQuad.normals[i * 3 + 2] = 1; }
	for (int i = 0; i < 6; ++i) SpriteQuad.indices[i] = I[i];
	UploadMesh(&SpriteQuad, false);

	BatchShader = LoadShaderFromMemory(BatchVS, BatchFS);
	BatchNormalShader = LoadShaderFromMemory(BatchVS, BatchNormalFS);
	if (BatchShader.id == rlGetShaderIdDefault() || BatchNormalShader.id == rlGetShaderIdDefault())
	{
		TraceLog(LOG_ERROR, "Toon: batch shader compilation failed");
		return false;
	}
	BatchShader.locs[SHADER_LOC_VERTEX_TEXCOORD02] = GetShaderLocationAttrib(BatchShader, "vertexTexCoord2");
	BatchNormalShader.locs[SHADER_LOC_VERTEX_TEXCOORD02] = GetShaderLocationAttrib(BatchNormalShader, "vertexTexCoord2");
	LocBLightDir = GetShaderLocation(BatchShader, "lightDir");
	LocBAmbient = GetShaderLocation(BatchShader, "ambient");
	LocBFogColor = GetShaderLocation(BatchShader, "fogColor");
	LocBFogDensity = GetShaderLocation(BatchShader, "fogDensity");
	LocBBands = GetShaderLocation(BatchShader, "bands");
	LocBViewPos = GetShaderLocation(BatchShader, "viewPos");
	BatchMaterial = LoadMaterialDefault();
	BatchMaterial.shader = BatchShader;
	BatchNormalMaterial = LoadMaterialDefault();
	BatchNormalMaterial.shader = BatchNormalShader;

	// Low-poly CPU copies of the primitives for batching (expanded triangles).
	{
		Mesh Src[static_cast<int>(MeshKind::Count)] = {
			GenMeshCube(1.0f, 1.0f, 1.0f), GenMeshCylinder(0.5f, 1.0f, 10), GenMeshSphere(0.5f, 6, 10),
			GenMeshCone(0.5f, 1.0f, 8), GenMeshCone(0.5f, 1.0f, 4) };
		RecenterY(Src[static_cast<int>(MeshKind::Cylinder)]);
		RecenterY(Src[static_cast<int>(MeshKind::Cone)]);
		RecenterY(Src[static_cast<int>(MeshKind::Pyramid)]);
		SquarePyramid(Src[static_cast<int>(MeshKind::Pyramid)]);
		for (int k = 0; k < static_cast<int>(MeshKind::Count); ++k)
		{
			const Mesh& M = Src[k];
			const int Count = M.indices ? M.triangleCount * 3 : M.vertexCount;
			for (int i = 0; i < Count; ++i)
			{
				const int Vi = M.indices ? M.indices[i] : i;
				for (int c = 0; c < 3; ++c)
				{
					LowPoly[k].V.push_back(M.vertices[Vi * 3 + c]);
					LowPoly[k].N.push_back(M.normals[Vi * 3 + c]);
				}
			}
			UnloadMesh(Src[k]);
		}
	}

	CelMaterial = LoadMaterialDefault();
	CelMaterial.shader = CelShader;
	NormalMaterial = LoadMaterialDefault();
	NormalMaterial.shader = NormalShader;

	Resize(Width, Height);
	bReady = true;
	return true;
}

void ToonRenderer::Resize(int Width, int Height)
{
	if (Width <= 0 || Height <= 0 || (Width == W && Height == H))
	{
		return;
	}
	DestroyTarget(ColorTarget);
	DestroyTarget(NormalTarget);
	W = Width;
	H = Height;
	ColorTarget = CreateTarget(W, H);
	NormalTarget = CreateTarget(W, H);
}

void ToonRenderer::Shutdown()
{
	if (!bReady)
	{
		return;
	}
	DestroyTarget(ColorTarget);
	DestroyTarget(NormalTarget);
	for (Mesh& M : Meshes)
	{
		UnloadMesh(M);
	}
	ClearStatic();
	UnloadShader(BatchShader);
	UnloadShader(BatchNormalShader);
	UnloadMesh(SpriteQuad);
	UnloadShader(SpriteShader);
	UnloadShader(SpriteNormalShader);
	UnloadShader(CelShader);
	UnloadShader(NormalShader);
	UnloadShader(OutlineShader);
	bReady = false;
}

void ToonRenderer::BeginPass(RenderPass InPass, const Camera3D& Cam, const SceneLighting& Light)
{
	Pass = InPass;
	if (Pass == RenderPass::Color)
	{
		BeginTextureMode(ColorTarget);
		ClearBackground(Light.SkyHorizon);
		// Vertical sky gradient behind the scene.
		DrawRectangleGradientV(0, 0, W, H / 2 + H / 6, Light.Sky, Light.SkyHorizon);

		const Vector3 SunDir = Vector3Normalize(Light.SunDirection);
		const Vector3 Ambient = ColorToVec(Light.Ambient);
		const Vector3 Fog = ColorToVec(Light.SkyHorizon);
		SetShaderValue(CelShader, LocLightDir, &SunDir, SHADER_UNIFORM_VEC3);
		SetShaderValue(CelShader, LocAmbient, &Ambient, SHADER_UNIFORM_VEC3);
		SetShaderValue(CelShader, LocFogColor, &Fog, SHADER_UNIFORM_VEC3);
		SetShaderValue(CelShader, LocFogDensity, &Light.FogDensity, SHADER_UNIFORM_FLOAT);
		SetShaderValue(CelShader, LocBands, &Light.Bands, SHADER_UNIFORM_FLOAT);
		SetShaderValue(CelShader, LocViewPos, &Cam.position, SHADER_UNIFORM_VEC3);
		SetShaderValue(BatchShader, LocBLightDir, &SunDir, SHADER_UNIFORM_VEC3);
		SetShaderValue(BatchShader, LocBAmbient, &Ambient, SHADER_UNIFORM_VEC3);
		SetShaderValue(BatchShader, LocBFogColor, &Fog, SHADER_UNIFORM_VEC3);
		SetShaderValue(BatchShader, LocBFogDensity, &Light.FogDensity, SHADER_UNIFORM_FLOAT);
		SetShaderValue(BatchShader, LocBBands, &Light.Bands, SHADER_UNIFORM_FLOAT);
		SetShaderValue(BatchShader, LocBViewPos, &Cam.position, SHADER_UNIFORM_VEC3);
		SetShaderValue(SpriteShader, LocSpriteFogColor, &Fog, SHADER_UNIFORM_VEC3);
		SetShaderValue(SpriteShader, LocSpriteFogDensity, &Light.FogDensity, SHADER_UNIFORM_FLOAT);
		SetShaderValue(SpriteShader, LocSpriteViewPos, &Cam.position, SHADER_UNIFORM_VEC3);
	}
	else
	{
		BeginTextureMode(NormalTarget);
		ClearBackground(BLANK);
		// Write normal + line weight verbatim (no alpha blending).
		rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
		BeginBlendMode(BLEND_CUSTOM);
	}
	Begin3DCustom(Cam, W, H);
}

void ToonRenderer::EndPass()
{
	EndMode3D();
	if (Pass == RenderPass::Normals)
	{
		EndBlendMode();
	}
	EndTextureMode();
}

void ToonRenderer::Draw(MeshKind Kind, const Matrix& Transform, Color Tint, float LineWeight, float Emissive)
{
	const Mesh& M = Meshes[static_cast<int>(Kind)];
	if (Pass == RenderPass::Color)
	{
		SetShaderValue(CelShader, LocEmissive, &Emissive, SHADER_UNIFORM_FLOAT);
		CelMaterial.maps[MATERIAL_MAP_DIFFUSE].color = Tint;
		DrawMesh(M, CelMaterial, Transform);
	}
	else
	{
		if (Tint.a < 255)
		{
			return; // translucent things don't get outlines
		}
		const float Weight = std::max(LineWeight, 0.05f) * 0.25f;
		SetShaderValue(NormalShader, LocLineWeight, &Weight, SHADER_UNIFORM_FLOAT);
		DrawMesh(M, NormalMaterial, Transform);
	}
}

void ToonRenderer::DrawSprite(const Texture2D& Tex, Vector3 Feet, Vector3 Right, Vector3 ToCamera, float Width, float Height,
                              float Roll, bool bFlip, Color Tint, float Flash, float LineWeight)
{
	const Vector3 Up{ 0.0f, 1.0f, 0.0f };
	const float C = std::cos(Roll), S = std::sin(Roll);
	// Lean: rotate the right/up axes about the view direction.
	Vector3 X = Vector3Add(Vector3Scale(Right, C), Vector3Scale(Up, S));
	Vector3 Y = Vector3Add(Vector3Scale(Right, -S), Vector3Scale(Up, C));
	X = Vector3Scale(X, Width * (bFlip ? -1.0f : 1.0f));
	Y = Vector3Scale(Y, Height);
	const Vector3 BL = Vector3Subtract(Feet, Vector3Scale(X, 0.5f));
	DrawSpriteQuad(Tex, BL, Vector3Add(BL, X), Vector3Add(BL, Y), ToCamera, Tint, Flash, LineWeight);
}

void ToonRenderer::DrawSpriteQuad(const Texture2D& Tex, Vector3 BL, Vector3 BR, Vector3 TL, Vector3 ToCamera,
                                  Color Tint, float Flash, float LineWeight)
{
	if (Tex.id == 0)
	{
		return;
	}
	// Unit quad spans x -0.5..0.5, y 0..1.
	const Vector3 X = Vector3Subtract(BR, BL);
	const Vector3 Y = Vector3Subtract(TL, BL);
	const Vector3 O = Vector3Add(BL, Vector3Scale(X, 0.5f));
	Matrix M{};
	M.m0 = X.x; M.m1 = X.y; M.m2 = X.z;
	M.m4 = Y.x; M.m5 = Y.y; M.m6 = Y.z;
	M.m8 = ToCamera.x; M.m9 = ToCamera.y; M.m10 = ToCamera.z;
	M.m12 = O.x; M.m13 = O.y; M.m14 = O.z;
	M.m15 = 1.0f;

	rlDisableBackfaceCulling();
	if (Pass == RenderPass::Color)
	{
		SetShaderValue(SpriteShader, LocSpriteFlash, &Flash, SHADER_UNIFORM_FLOAT);
		SpriteMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = Tex;
		SpriteMaterial.maps[MATERIAL_MAP_DIFFUSE].color = Tint;
		DrawMesh(SpriteQuad, SpriteMaterial, M);
	}
	else if (Tint.a == 255)
	{
		const float Weight = std::max(LineWeight, 0.05f) * 0.25f;
		SetShaderValue(SpriteNormalShader, LocSpriteLineWeight, &Weight, SHADER_UNIFORM_FLOAT);
		SpriteNormalMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = Tex;
		DrawMesh(SpriteQuad, SpriteNormalMaterial, M);
	}
	rlEnableBackfaceCulling();
}

void ToonRenderer::BeginStaticBatch()
{
	Building.clear();
}

void ToonRenderer::AddStatic(MeshKind Kind, const Matrix& T, Color Tint, float Emissive, float LineWeight)
{
	const CpuMesh& Src = LowPoly[static_cast<int>(Kind)];
	const Matrix NM = MatrixTranspose(MatrixInvert(T));
	const long long Cx = static_cast<long long>(std::floor(T.m12 / 80.0f));
	const long long Cz = static_cast<long long>(std::floor(T.m14 / 80.0f));
	BatchBuild& B = Building[(Cx << 32) ^ (Cz & 0xffffffff)];
	const size_t Count = Src.V.size() / 3;
	for (size_t i = 0; i < Count; ++i)
	{
		const float X = Src.V[i * 3], Y = Src.V[i * 3 + 1], Z = Src.V[i * 3 + 2];
		B.V.push_back(T.m0 * X + T.m4 * Y + T.m8 * Z + T.m12);
		B.V.push_back(T.m1 * X + T.m5 * Y + T.m9 * Z + T.m13);
		B.V.push_back(T.m2 * X + T.m6 * Y + T.m10 * Z + T.m14);
		const float Nx = Src.N[i * 3], Ny = Src.N[i * 3 + 1], Nz = Src.N[i * 3 + 2];
		Vector3 N{ NM.m0 * Nx + NM.m4 * Ny + NM.m8 * Nz, NM.m1 * Nx + NM.m5 * Ny + NM.m9 * Nz, NM.m2 * Nx + NM.m6 * Ny + NM.m10 * Nz };
		N = Vector3Normalize(N);
		B.N.push_back(N.x);
		B.N.push_back(N.y);
		B.N.push_back(N.z);
		B.C.push_back(Tint.r);
		B.C.push_back(Tint.g);
		B.C.push_back(Tint.b);
		B.C.push_back(255);
		B.T2.push_back(Emissive);
		B.T2.push_back(std::max(LineWeight, 0.05f) * 0.25f);
	}
}

void ToonRenderer::EndStaticBatch()
{
	for (auto& Pair : Building)
	{
		BatchBuild& B = Pair.second;
		const int Verts = static_cast<int>(B.V.size() / 3);
		if (Verts == 0)
		{
			continue;
		}
		BatchChunk C;
		C.M.vertexCount = Verts;
		C.M.triangleCount = Verts / 3;
		C.M.vertices = static_cast<float*>(MemAlloc(B.V.size() * sizeof(float)));
		C.M.normals = static_cast<float*>(MemAlloc(B.N.size() * sizeof(float)));
		C.M.colors = static_cast<unsigned char*>(MemAlloc(B.C.size()));
		C.M.texcoords2 = static_cast<float*>(MemAlloc(B.T2.size() * sizeof(float)));
		std::memcpy(C.M.vertices, B.V.data(), B.V.size() * sizeof(float));
		std::memcpy(C.M.normals, B.N.data(), B.N.size() * sizeof(float));
		std::memcpy(C.M.colors, B.C.data(), B.C.size());
		std::memcpy(C.M.texcoords2, B.T2.data(), B.T2.size() * sizeof(float));
		UploadMesh(&C.M, false);
		Vector3 Min{ 1e9f, 1e9f, 1e9f }, Max{ -1e9f, -1e9f, -1e9f };
		for (int i = 0; i < Verts; ++i)
		{
			Min.x = std::min(Min.x, B.V[i * 3]); Max.x = std::max(Max.x, B.V[i * 3]);
			Min.y = std::min(Min.y, B.V[i * 3 + 1]); Max.y = std::max(Max.y, B.V[i * 3 + 1]);
			Min.z = std::min(Min.z, B.V[i * 3 + 2]); Max.z = std::max(Max.z, B.V[i * 3 + 2]);
		}
		C.Center = Vector3Scale(Vector3Add(Min, Max), 0.5f);
		C.Radius = Vector3Length(Vector3Subtract(Max, Min)) * 0.5f;
		Chunks.push_back(C);
		StaticVerts += Verts;
	}
	Building.clear();
}

void ToonRenderer::ClearStatic()
{
	for (BatchChunk& C : Chunks)
	{
		UnloadMesh(C.M);
	}
	Chunks.clear();
	StaticVerts = 0;
}

void ToonRenderer::DrawStatic(Vector3 CamPos, Vector3 CamForward, float MaxDistance)
{
	const Material& Mat = Pass == RenderPass::Color ? BatchMaterial : BatchNormalMaterial;
	for (const BatchChunk& C : Chunks)
	{
		const Vector3 To = Vector3Subtract(C.Center, CamPos);
		if (Vector3Length(To) - C.Radius > MaxDistance || Vector3DotProduct(To, CamForward) < -C.Radius)
		{
			continue;
		}
		DrawMesh(C.M, Mat, MatrixIdentity());
	}
}

void ToonRenderer::Composite(const SceneLighting& Light)
{
	const Vector2 Texel{ 1.0f / static_cast<float>(W), 1.0f / static_cast<float>(H) };
	const Vector2 NearFar{ NearPlane, FarPlane };
	const Vector3 Sky = ColorToVec(Light.Sky);
	BeginShaderMode(OutlineShader);
	SetShaderValue(OutlineShader, LocTexel, &Texel, SHADER_UNIFORM_VEC2);
	SetShaderValue(OutlineShader, LocNearFar, &NearFar, SHADER_UNIFORM_VEC2);
	SetShaderValue(OutlineShader, LocSkyTop, &Sky, SHADER_UNIFORM_VEC3);
	static const int DebugView = getenv("TOON_DEBUG_VIEW") ? atoi(getenv("TOON_DEBUG_VIEW")) : 0;
	SetShaderValue(OutlineShader, GetShaderLocation(OutlineShader, "debugView"), &DebugView, SHADER_UNIFORM_INT);
	SetShaderValueTexture(OutlineShader, LocNormalTex, NormalTarget.texture);
	SetShaderValueTexture(OutlineShader, LocDepthTex, NormalTarget.depth);
	DrawTextureRec(ColorTarget.texture, Rectangle{ 0.0f, 0.0f, static_cast<float>(W), -static_cast<float>(H) }, Vector2{ 0.0f, 0.0f }, WHITE);
	EndShaderMode();
}
