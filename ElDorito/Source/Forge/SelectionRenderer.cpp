#include "SelectionRenderer.hpp"
#include "../Blam/Math/RealMatrix4x3.hpp"
#include "../Blam/BlamObjects.hpp"
#include "../Blam/BlamTime.hpp"
#include "../Forge/ForgeUtil.hpp"
#include "../Forge/Selection.hpp"
#include "../Forge/ObjectSet.hpp"
#include "../Blam/Tags/TagInstance.hpp"
#include "../Blam/Math/MathUtil.hpp"
#include "../Patch.hpp"
#include <cstdint>

using namespace Blam::Math;

using namespace Forge::SelectionRenderer;

namespace
{
	struct VertexPosUV { float X, Y, Z, U, V; };
	const VertexPosUV BOX_VERTS[] =
	{
		// front
		{ -0.5f, 0.5f,-0.5f,  0.0f,  0.0f },
		{ 0.5f, 0.5f,-0.5f,  0.5f,  0.0f },
		{ -0.5f,-0.5f,-0.5f,  0.0f,  0.5f },
		{ 0.5f,-0.5f,-0.5f,  0.5f,  0.5f },
		// back
		{ -0.5f, 0.5f, 0.5f,  0.5f,  0.0f },
		{ -0.5f,-0.5f, 0.5f,  0.5f,  0.5f },
		{ 0.5f, 0.5f, 0.5f,  0.0f,  0.0f },
		{ 0.5f,-0.5f, 0.5f,  0.0f,  0.5f },
		// top
		{ -0.5f, 0.5f, 0.5f,  0.0f,  0.0f },
		{ 0.5f, 0.5f, 0.5f,  0.5f,  0.0f },
		{ -0.5f, 0.5f,-0.5f,  0.0f,  0.5f },
		{ 0.5f, 0.5f,-0.5f,  0.5f,  0.5f },
		// bottom
		{ -0.5f,-0.5f, 0.5f,  0.0f,  0.0f },
		{ -0.5f,-0.5f,-0.5f,  0.5f,  0.0f },
		{ 0.5f,-0.5f, 0.5f,  0.0f,  0.5f },
		{ 0.5f,-0.5f,-0.5f,  0.5f,  0.5f },
		// right
		{ 0.5f, 0.5f,-0.5f,  0.0f,  0.0f },
		{ 0.5f, 0.5f, 0.5f,  0.5f,  0.0f },
		{ 0.5f,-0.5f,-0.5f,  0.0f,  0.5f },
		{ 0.5f,-0.5f, 0.5f,  0.5f,  0.5f },
		// left
		{ -0.5f, 0.5f,-0.5f,  0.5f,  0.0f },
		{ -0.5f,-0.5f,-0.5f,  0.5f,  0.5f },
		{ -0.5f, 0.5f, 0.5f,  0.0f,  0.0f },
		{ -0.5f,-0.5f, 0.5f,  0.0f,  0.5f }
	};

	struct SelectionItem
	{
		RealMatrix4x3 Transform;
		float Radius;
		float Width;
		float Depth;
		float Height;
	};

	const int MAX_ITEMS = 256;

	int s_RendererType = 0;
	bool s_Enabled = false;
	int s_NumActiveItems;
	float s_SelectionOpacity = 0.5f;
	float s_SelectionColorCounter = 0.0f;
	SelectionItem s_items[MAX_ITEMS];

	void RasterizeImplicitGeometryHook();
	void SpecialWeaponHUDHook(int a1, uint32_t unitObjectIndex, int a3, uint32_t* objectsInCluster, int16_t objectcount, BYTE* activeSpecialChudTypes);
}

namespace Forge
{
	void SelectionRenderer::Initialize()
	{
		Hook(0x62E760, SpecialWeaponHUDHook, HookFlags::IsCall).Apply();
		Hook(0x63B409, RasterizeImplicitGeometryHook, HookFlags::IsCall).Apply();
	}

	void SelectionRenderer::Update()
	{
		if (s_RendererType == eRendererImplicit)
		{
			s_NumActiveItems = 0;

			const auto& selection = Forge::Selection::GetSelection();
			auto& objects = Blam::Objects::GetObjects();
			for (auto it = objects.begin(); it != objects.end() && s_NumActiveItems < MAX_ITEMS; ++it)
			{
				if (!selection.Contains(it.CurrentDatumIndex))
					continue;

				auto& item = s_items[s_NumActiveItems++];

				Forge::AABB boundingBox;
				Forge::GetObjectTransformationMatrix(it.CurrentDatumIndex, &item.Transform);
				Forge::CalculateObjectBoundingBox(it.CurrentDatumIndex, &boundingBox);

				item.Transform.Forward *= (boundingBox.MaxX - boundingBox.MinX) * 1.01f;
				item.Transform.Left *= (boundingBox.MaxY - boundingBox.MinY) * 1.01f;
				item.Transform.Up *= (boundingBox.MaxZ - boundingBox.MinZ) * 1.01f;
				item.Transform.Position = it->Data->Center;

				item.Width = boundingBox.MaxX - boundingBox.MinX;
				item.Depth = boundingBox.MaxY - boundingBox.MinY;
				item.Height = boundingBox.MaxZ - boundingBox.MinZ;
			}

			s_SelectionColorCounter += Blam::Time::GetSecondsPerTick() / 1.0f;
			if (s_SelectionColorCounter > PI * 2.0f)
				s_SelectionColorCounter -= PI * 2.0f;
		}
	}

	void SelectionRenderer::SetEnabled(bool enabled)
	{
		s_Enabled = enabled;
	}

	void SelectionRenderer::SetRendererType(RendererImplementationType type)
	{
		s_RendererType = type;
	}
}

namespace
{
	void SpecialWeaponHUDHook(int a1, uint32_t unitObjectIndex, int a3, uint32_t* objectsInCluster, int16_t objectcount, BYTE* activeSpecialChudTypes)
	{
		if (s_RendererType != eRendererSpecialHud)
			return;

		static auto sub_A2CAA0 = (void(__cdecl*)(int a1, uint32_t unitObjectIndex, int a3, uint32_t* objects, int16_t objectCount, BYTE *result))(0xA2CAA0);

		static auto sub_686FD0 = (void(__cdecl*)())(0x686FD0);
		static auto sub_A78230 = (void(__cdecl*)())(0xA78230);
		static auto sub_A53160 = (void(__cdecl*)(int a1, int a2, char a3, char a4))(0xA53160);
		static auto sub_A7A510 = (void(__cdecl*)(int a1))(0xA7A510);
		static auto sub_A242E0 = (unsigned __int8(__cdecl*)(int a1, int a2))(0xA242E0);
		static auto sub_A48E40 = (int(__cdecl*)(int a1, int a2, int a3))(0xA48E40);
		static auto sub_A78F00 = (int(__cdecl*)(int a1, int a2))(0xA78F00);
		static auto sub_A781F0 = (int(*)())(0xA781F0);
		static auto sub_686DE0 = (int(*)())(0x686DE0);

		sub_A2CAA0(a1, unitObjectIndex, a3, objectsInCluster, objectcount, activeSpecialChudTypes);

		if (!s_Enabled)
			return;

		auto mapv = Forge::GetMapVariant();
		if (!mapv)
			return;

		const auto& selection = Forge::Selection::GetSelection();
		if (!selection.Any())
			return;

		for (auto i = 0; i < mapv->UsedPlacementsCount; i++)
		{
			auto& placement = mapv->Placements[i];

			if (selection.Contains(placement.ObjectIndex))
			{
				const int specialHudType = 6;
				activeSpecialChudTypes[specialHudType] = 1;

				sub_686FD0();
				sub_A78230();
				sub_A53160(placement.ObjectIndex, a1, 0, 1);
				sub_A7A510(0);
				sub_A242E0(
					18,
					(unsigned __int8)(signed int)floor((float)((float)(signed int)specialHudType * 255.0) * 0.125));
				sub_A48E40(2, 23, -1);
				sub_A78F00(0, 1);
				sub_A48E40(1, 0, 0);
				sub_A48E40(2, 0, 0);
				sub_A781F0();
				sub_686DE0();
			}
		}
	}
	void RenderImplicit()
	{
		if (s_RendererType != eRendererImplicit)
			return;

		static const auto UseDefaultShader = (bool(*)(int defaultShaderIndex, int a2, int a3, int a4))(0x00A23300);
		static const auto SetArgument = (void(*)(int id, signed int type, void *value))(0x00A66410);
		static const auto DrawPrimitive = (int(*)(int primitiveType, int primitiveCount, void *data, int stride))(0x00A28330);
		static const auto sub_A3CA60 = (void(*)(uint32_t shaderTagIndex, void* shaderDef, int a3, unsigned int a4, int a5, int a6))(0xA3CA60);
		static const auto sub_A232D0 = (int(*)(int a1))(0xA232D0);
		static const auto sub_A22BA0 = (int(*)())(0xA22BA0);

		const auto PT_TRIANGLESTRIP = 5;
		const auto SHADER_TAGINDEX = 0x303c;

		if (!s_Enabled || !UseDefaultShader(64, 0x14, 0, 0))
			return;

		auto shaderDef = Blam::Tags::TagInstance(SHADER_TAGINDEX).GetDefinition<void>();
		sub_A3CA60(SHADER_TAGINDEX, shaderDef, 20, 0, PT_TRIANGLESTRIP, 1);

		for (auto i = 0; i < s_NumActiveItems; i++)
		{
			const auto& item = s_items[i];

			float m[12];
			m[0] = item.Transform.Forward.I;
			m[1] = item.Transform.Left.I;
			m[2] = item.Transform.Up.I;
			m[3] = item.Transform.Position.I;
			m[4] = item.Transform.Forward.J;
			m[5] = item.Transform.Left.J;
			m[6] = item.Transform.Up.J;
			m[7] = item.Transform.Position.J;
			m[8] = item.Transform.Forward.K;
			m[9] = item.Transform.Left.K;
			m[10] = item.Transform.Up.K;
			m[11] = item.Transform.Position.K;

			// rainbow because we can
			float color[] =
			{
				std::sin(s_SelectionColorCounter + 2) * 0.5f + 0.5f,
				std::sin(s_SelectionColorCounter + 0) * 0.5f + 0.5f,
				std::sin(s_SelectionColorCounter + 4) * 0.5f + 0.5f,
				1.0f
			};

			SetArgument(24, 3, (void*)m);
			SetArgument(20, 1, color);

			auto v14 = sub_A22BA0();
			sub_A232D0(1);

			DrawPrimitive(PT_TRIANGLESTRIP, 2, (void*)BOX_VERTS, sizeof(VertexPosUV));
			DrawPrimitive(PT_TRIANGLESTRIP, 2, (void*)(BOX_VERTS + 4), sizeof(VertexPosUV));
			DrawPrimitive(PT_TRIANGLESTRIP, 2, (void*)(BOX_VERTS + 8), sizeof(VertexPosUV));
			DrawPrimitive(PT_TRIANGLESTRIP, 2, (void*)(BOX_VERTS + 12), sizeof(VertexPosUV));
			DrawPrimitive(PT_TRIANGLESTRIP, 2, (void*)(BOX_VERTS + 16), sizeof(VertexPosUV));
			DrawPrimitive(PT_TRIANGLESTRIP, 2, (void*)(BOX_VERTS + 20), sizeof(VertexPosUV));
		}
	}

	void RasterizeImplicitGeometryHook()
	{
		static const auto RasterizeImplicitGeometry = (void(*)())(0x00A743B0);
		RasterizeImplicitGeometry();
		RenderImplicit();
	}
}