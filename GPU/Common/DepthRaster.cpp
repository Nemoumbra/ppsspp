#include <algorithm>
#include <cstring>
#include <cstdint>

#include "Common/Math/CrossSIMD.h"
#include "GPU/Common/DepthRaster.h"
#include "GPU/Math3D.h"
#include "Common/Math/math_util.h"
#include "GPU/Common/VertexDecoderCommon.h"

// We only need to support these three modes.
enum class ZCompareMode {
	Greater,  // Most common
	Less,  // Less common
	Always,  // Fairly common
};

void DepthRasterRect(uint16_t *dest, int stride, int x1, int y1, int x2, int y2, short depthValue, ZCompareMode compareMode) {
	// Swap coordinates if needed, we don't back-face-cull rects.
	// We also ignore the UV rotation here.
	if (x1 > x2) {
		std::swap(x1, x2);
	}
	if (y1 > y2) {
		std::swap(y1, y2);
	}
	if (x1 == x2 || y1 == y2) {
		return;
	}

	Vec8U16 valueX8 = Vec8U16::Splat(depthValue);
	for (int y = y1; y < y2; y++) {
		uint16_t *ptr = (uint16_t *)(dest + stride * y + x1);
		int w = x2 - x1;
		switch (compareMode) {
		case ZCompareMode::Always:
			if (depthValue == 0) {
				memset(ptr, 0, w * 2);
			} else {
				while (w >= 8) {
					valueX8.Store(ptr);
					ptr += 8;
					w -= 8;
				}
			}
			break;
			// TODO: Trailer
		default:
			// TODO
			break;
		}
	}
}

alignas(16) static const int zero123[4]  = {0, 1, 2, 3};

struct Edge {
	// Dimensions of our pixel group
	static const int stepXSize = 4;
	static const int stepYSize = 1;

	Vec4S32 oneStepX;
	Vec4S32 oneStepY;

	Vec4S32 init(int v0x, int v0y, int v1x, int v1y, int p0x, int p0y) {
		// Edge setup
		int A = v0y - v1y;
		int B = v1x - v0x;
		int C = v0x * v1y - v0y * v1x;

		// Step deltas
		oneStepX = Vec4S32::Splat(A * stepXSize);
		oneStepY = Vec4S32::Splat(B * stepYSize);

		// x/y values for initial pixel block. Add horizontal offsets.
		Vec4S32 x = Vec4S32::Splat(p0x) + Vec4S32::LoadAligned(zero123);
		Vec4S32 y = Vec4S32::Splat(p0y);

		// Edge function values at origin
		return Vec4S32::Splat(A) * x + Vec4S32::Splat(B) * y + Vec4S32::Splat(C);
	}
};

// Adapted from Intel's depth rasterizer example.
// Started with the scalar version, will SIMD-ify later.
// x1/y1 etc are the scissor rect.
void DepthRasterTriangle(uint16_t *depthBuf, int stride, int x1, int y1, int x2, int y2, const int *tx, const int *ty, const int *tz, ZCompareMode compareMode) {
	int tileStartX = x1;
	int tileEndX = x2;

	int tileStartY = y1;
	int tileEndY = y2;

	// BEGIN triangle setup. This should be done SIMD, four triangles at a time.
	// Due to the many multiplications, we might want to do it in floating point as 32-bit integer muls
	// are slow on SSE2.

	// Convert to whole pixels for now. Later subpixel precision.
	int v0x = tx[0];
	int v0y = ty[0];
	int v0z = tz[0];
	int v1x = tx[1];
	int v1y = ty[1];
	int v1z = tz[1];
	int v2x = tx[2];
	int v2y = ty[2];
	int v2z = tz[2];

	// use fixed-point only for X and Y.  Avoid work for Z and W.
	// We use 4x1 tiles for simplicity.
	int minX = std::max(std::min(std::min(v0x, v1x), v2x), tileStartX) & ~3;
	int maxX = std::min(std::max(std::max(v0x, v1x), v2x) + 3, tileEndX) & ~3;
	int minY = std::max(std::min(std::min(v0y, v1y), v2y), tileStartY);
	int maxY = std::min(std::max(std::max(v0y, v1y), v2y), tileEndY);
	if (maxX == minX || maxY == minY) {
		// No pixels, or outside screen.
		return;
	}

	// TODO: Cull really small triangles here.
	int triArea = (v1y - v2y) * v0x + (v2x - v1x) * v0y + (v1x * v2y - v2x * v1y);
	if (triArea <= 0) {
		return;
	}

	float oneOverTriArea = 1.0f / (float)triArea;

	Edge e01, e12, e20;

	Vec4S32 w0_row = e12.init(v1x, v1y, v2x, v2y, minX, minY);
	Vec4S32 w1_row = e20.init(v2x, v2y, v0x, v0y, minX, minY);
	Vec4S32 w2_row = e01.init(v0x, v0y, v1x, v1y, minX, minY);

	// Prepare to interpolate Z
	Vec4F32 zz0 = Vec4F32::Splat((float)v0z);
	Vec4F32 zz1 = Vec4F32::Splat((float)(v1z - v0z) * oneOverTriArea);
	Vec4F32 zz2 = Vec4F32::Splat((float)(v2z - v0z) * oneOverTriArea);

	// Rasterize
	for (int y = minY; y <= maxY; y += Edge::stepYSize, w0_row += e12.oneStepY, w1_row += e20.oneStepY, w2_row += e01.oneStepY) {
		// Barycentric coordinates at start of row
		Vec4S32 w0 = w0_row;
		Vec4S32 w1 = w1_row;
		Vec4S32 w2 = w2_row;

		uint16_t *rowPtr = depthBuf + stride * y;

		for (int x = minX; x <= maxX; x += Edge::stepXSize, w0 += e12.oneStepX, w1 += e20.oneStepX, w2 += e01.oneStepX) {
			// If p is on or inside all edges for any pixels,
			// render those pixels.
			Vec4S32 signCalc = w0 | w1 | w2;
			if (!AnyZeroSignBit(signCalc)) {
				continue;
			}

			Vec4U16 bufferValues = Vec4U16::Load(rowPtr + x);
			Vec4U16 shortMaskInv = SignBits32ToMaskU16(signCalc);
			// Now, the mask has 1111111 where we should preserve the contents of the depth buffer.

			// Compute the Z value for all four pixels.
			// float depth = zz[0] + beta * zz[1] + gamma * zz[2];
			Vec4U16 shortZ = Vec4U16::FromVec4F32(zz0 + Vec4F32FromS32(w1) * zz1 + Vec4F32FromS32(w2) * zz2);

			// TODO: Lift this switch out of the inner loop, or even out of the function with templating.
			switch (compareMode) {
			case ZCompareMode::Greater:
				// To implement the greater/greater-than comparison, we can combine mask and max.
				// It might be better to do the math in float space on x86 due to SSE2 deficiencies.
				// We use AndNot to zero out Z results, before doing Max with the buffer.
				AndNot(shortZ, shortMaskInv).Max(bufferValues).Store(rowPtr + x);
				break;
			case ZCompareMode::Less:  // UNTESTED
				// This time, we OR the mask and use .Min.
				(shortZ | shortMaskInv).Min(bufferValues).Store(rowPtr + x);
				break;
			case ZCompareMode::Always:  // UNTESTED
				// This could be replaced with a vblend operation.
				((bufferValues & shortMaskInv) | AndNot(shortZ, shortMaskInv)).Store(rowPtr + x);
				break;
			}
		}
	}
}

void DecodeAndTransformForDepthRaster(float *dest, GEPrimitiveType prim, const float *worldviewproj, const void *vertexData, int indexLowerBound, int indexUpperBound, VertexDecoder *dec, u32 vertTypeID) {
	// TODO: Ditch skinned and morphed prims for now since we don't have a fast way to skin without running the full decoder.
	_dbg_assert_((vertTypeID & (GE_VTYPE_WEIGHT_MASK | GE_VTYPE_MORPHCOUNT_MASK)) == 0);

	int vertexStride = dec->VertexSize();
	int offset = dec->posoff;

	Mat4F32 mat(worldviewproj);

	const u8 *startPtr = (const u8 *)vertexData + indexLowerBound * vertexStride;
	int count = indexUpperBound - indexLowerBound + 1;

	switch (vertTypeID & GE_VTYPE_POS_MASK) {
	case GE_VTYPE_POS_FLOAT:
		for (int i = 0; i < count; i++) {
			const float *data = (const float *)(startPtr + i * vertexStride + offset);
			Vec4F32::Load(data).AsVec3ByMatrix44(mat).Store(dest + i * 4);
		}
		break;
	case GE_VTYPE_POS_16BIT:
		for (int i = 0; i < count; i++) {
			const s16 *data = ((const s16 *)((const s8 *)startPtr + i * vertexStride + offset));
			Vec4F32::LoadConvertS16(data).Mul(1.0f / 32768.f).AsVec3ByMatrix44(mat).Store(dest + i * 4);
		}
		break;
	case GE_VTYPE_POS_8BIT:
		for (int i = 0; i < count; i++) {
			const s8 *data = (const s8 *)startPtr + i * vertexStride + offset;
			Vec4F32::LoadConvertS8(data).Mul(1.0f / 128.0f).AsVec3ByMatrix44(mat).Store(dest + i * 4);
		}
		break;
	}
}

int DepthRasterClipIndexedTriangles(int *tx, int *ty, int *tz, const float *transformed, const uint16_t *indexBuffer, int count) {
	bool cullEnabled = gstate.isCullEnabled();
	GECullMode cullMode = gstate.getCullMode();

	// TODO: On ARM we can do better by keeping these in lanes instead of splatting.
	// However, hard to find a common abstraction.
	const Vec4F32 viewportX = Vec4F32::Splat(gstate.getViewportXCenter());
	const Vec4F32 viewportY = Vec4F32::Splat(gstate.getViewportYCenter());
	const Vec4F32 viewportZ = Vec4F32::Splat(gstate.getViewportZCenter());
	const Vec4F32 viewportScaleX = Vec4F32::Splat(gstate.getViewportXScale());
	const Vec4F32 viewportScaleY = Vec4F32::Splat(gstate.getViewportYScale());
	const Vec4F32 viewportScaleZ = Vec4F32::Splat(gstate.getViewportZScale());

	const Vec4F32 offsetX = Vec4F32::Splat(gstate.getOffsetX());  // We remove the 16 scale here
	const Vec4F32 offsetY = Vec4F32::Splat(gstate.getOffsetY());

	bool cullCCW = false;

	int outCount = 0;

	int flipCull = 0;
	if (cullEnabled && cullMode == GE_CULL_CW) {
		flipCull = 3;
	}
	for (int i = 0; i < count; i += 3) {
		const float *verts[3] = {
			transformed + indexBuffer[i] * 4,
			transformed + indexBuffer[i + (1 ^ flipCull)] * 4,
			transformed + indexBuffer[i + (2 ^ flipCull)] * 4,
		};

		// Check if any vertex is behind the 0 plane.
		if (verts[0][3] < 0.0f || verts[1][3] < 0.0f || verts[2][3] < 0.0f) {
			// Ditch this triangle. Later we should clip here.
			continue;
		}

		// These names are wrong .. until we transpose.
		Vec4F32 x = Vec4F32::Load(verts[0]);
		Vec4F32 y = Vec4F32::Load(verts[1]);
		Vec4F32 z = Vec4F32::Load(verts[2]);
		Vec4F32 w = Vec4F32::Zero();
		Vec4F32::Transpose(x, y, z, w);
		// Now the names are accurate! Since we only have three vertices, the fourth member of each vector is zero
		// and will not be stored (well it will be stored, but it'll be overwritten by the next vertex).
		Vec4F32 recipW = w.Recip();

		x *= recipW;
		y *= recipW;
		z *= recipW;

		Vec4S32 screen[3];
		screen[0] = Vec4S32FromF32((x * viewportScaleX + viewportX) - offsetX);
		screen[1] = Vec4S32FromF32((y * viewportScaleY + viewportY) - offsetY);
		screen[2] = Vec4S32FromF32((z * viewportScaleZ + viewportZ).Clamp(0.0f, 65535.0f));

		screen[0].Store(tx + outCount);
		screen[1].Store(ty + outCount);
		screen[2].Store(tz + outCount);
		outCount += 3;

		if (!cullEnabled) {
			// If culling is off, shuffle the three vectors to produce the opposite triangle, and store them after.

			// HOWEVER! I realized that this is not the optimal layout, after all.
			// We should group 4 triangles at a time and interleave them (so we first have all X of vertex 0,
			// then all X of vertex 1, and so on). This seems solvable with another transpose, if we can easily
			// collect four triangles at a time...

			screen[0].SwapLowerElements().Store(tx + outCount);
			screen[1].SwapLowerElements().Store(ty + outCount);
			screen[2].SwapLowerElements().Store(tz + outCount);
			outCount += 3;
		}
	}
	return outCount;
}

void DepthRasterConvertTransformed(int *tx, int *ty, int *tz, GEPrimitiveType prim, const TransformedVertex *transformed, int count) {
	_dbg_assert_(prim == GE_PRIM_RECTANGLES || prim == GE_PRIM_TRIANGLES);

	// TODO: This is basically a transpose, or AoS->SoA conversion. There may be fast ways.
	for (int i = 0; i < count; i++) {
		tx[i] = (int)transformed[i].pos[0];
		ty[i] = (int)transformed[i].pos[1];
		tz[i] = (u16)transformed[i].pos[2];
	}
}

// Rasterizes screen-space vertices.
void DepthRasterScreenVerts(uint16_t *depth, int depthStride, GEPrimitiveType prim, int x1, int y1, int x2, int y2, const int *tx, const int *ty, const int *tz, int count) {
	// Prim should now be either TRIANGLES or RECTs.
	_dbg_assert_(prim == GE_PRIM_RECTANGLES || prim == GE_PRIM_TRIANGLES);

	// Ignore draws where stencil operations are active?
	if (gstate.isStencilTestEnabled()) {
		// return;
	}

	GEComparison compareMode = gstate.getDepthTestFunction();

	ZCompareMode comp;
	// Ignore some useless compare modes.
	switch (compareMode) {
	case GE_COMP_ALWAYS:
		comp = ZCompareMode::Always;
		break;
	case GE_COMP_LEQUAL:
	case GE_COMP_LESS:
		comp = ZCompareMode::Less;
		break;
	case GE_COMP_GEQUAL:
	case GE_COMP_GREATER:
		comp = ZCompareMode::Greater;  // Most common
		break;
	case GE_COMP_NEVER:
	case GE_COMP_EQUAL:
		// These will never have a useful effect in Z-only raster.
		[[fallthrough]];
	case GE_COMP_NOTEQUAL:
		// This is highly unusual, let's just ignore it.
		[[fallthrough]];
	default:
		return;
	}

	if (gstate.isModeClear()) {
		if (!gstate.isClearModeDepthMask()) {
			return;
		}
		comp = ZCompareMode::Always;
	} else {
		if (!gstate.isDepthTestEnabled() || !gstate.isDepthWriteEnabled())
			return;
	}

	switch (prim) {
	case GE_PRIM_RECTANGLES:
		for (int i = 0; i < count; i += 2) {
			uint16_t z = tz[i + 1];  // depth from second vertex
			// TODO: Should clip coordinates to the scissor rectangle.
			// We remove the subpixel information here.
			DepthRasterRect(depth, depthStride, tx[i], ty[i], tx[i + 1], ty[i + 1], z, comp);
		}
		break;
	case GE_PRIM_TRIANGLES:
		for (int i = 0; i < count; i += 3) {
			DepthRasterTriangle(depth, depthStride, x1, y1, x2, y2, &tx[i], &ty[i], &tz[i], comp);
		}
		break;
	default:
		_dbg_assert_(false);
	}
}