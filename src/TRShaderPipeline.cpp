#include "TRShaderPipeline.h"

#include <algorithm>

namespace TinyRenderer
{
	//----------------------------------------------VertexData----------------------------------------------

	TRShaderPipeline::VertexData TRShaderPipeline::VertexData::lerp(
		const TRShaderPipeline::VertexData &v0,
		const TRShaderPipeline::VertexData &v1,
		float frac)
	{
		//Linear interpolation
		VertexData result;
		result.pos = (1.0f - frac) * v0.pos + frac * v1.pos;
		result.col = (1.0f - frac) * v0.col + frac * v1.col;
		result.nor = (1.0f - frac) * v0.nor + frac * v1.nor;
		result.tex = (1.0f - frac) * v0.tex + frac * v1.tex;
		result.cpos = (1.0f - frac) * v0.cpos + frac * v1.cpos;
		result.spos.x = (1.0f - frac) * v0.spos.x + frac * v1.spos.x;
		result.spos.y = (1.0f - frac) * v0.spos.y + frac * v1.spos.y;

		return result;
	}

	TRShaderPipeline::VertexData TRShaderPipeline::VertexData::barycentricLerp(
		const VertexData &v0, 
		const VertexData &v1, 
		const VertexData &v2,
		glm::vec3 w)
	{
		VertexData result;
		result.pos = w.x * v0.pos + w.y * v1.pos + w.z * v2.pos;
		result.col = w.x * v0.col + w.y * v1.col + w.z * v2.col;
		result.nor = w.x * v0.nor + w.y * v1.nor + w.z * v2.nor;
		result.tex = w.x * v0.tex + w.y * v1.tex + w.z * v2.tex;
		result.cpos = w.x * v0.cpos + w.y * v1.cpos + w.z * v2.cpos;
		result.spos.x = w.x * v0.spos.x + w.y * v1.spos.x + w.z * v2.spos.x;
		result.spos.y = w.x * v0.spos.y + w.y * v1.spos.y + w.z * v2.spos.y;

		return result;
	}

	void TRShaderPipeline::VertexData::prePerspCorrection(VertexData &v)
	{
		//Perspective correction: the world space properties should be multipy by 1/w before rasterization
		//https://zhuanlan.zhihu.com/p/144331875
		//We use pos.w to store 1/w
		v.pos.w = 1.0f / v.cpos.w;
		v.pos = glm::vec4(v.pos.x * v.pos.w, v.pos.y * v.pos.w, v.pos.z * v.pos.w, v.pos.w);
		v.tex = v.tex * v.pos.w;
		v.nor = v.nor * v.pos.w;
		v.col = v.col * v.pos.w;
	}

	void TRShaderPipeline::VertexData::aftPrespCorrection(VertexData &v)
	{
		//Perspective correction: the world space properties should be multipy by w after rasterization
		//https://zhuanlan.zhihu.com/p/144331875
		//We use pos.w to store 1/w
		float w = 1.0f / v.pos.w;
		v.pos = v.pos * w;
		v.tex = v.tex * w;
		v.nor = v.nor * w;
		v.col = v.col * w;
	}

	//----------------------------------------------TRShaderPipeline----------------------------------------------

	void TRShaderPipeline::rasterize_wire(
		const VertexData &v0,
		const VertexData &v1,
		const VertexData &v2,
		const unsigned int &screen_width,
		const unsigned int &screene_height,
		std::vector<VertexData> &rasterized_points)
	{
		//Draw each line step by step
		rasterize_wire_aux(v0, v1, screen_width, screene_height, rasterized_points);
		rasterize_wire_aux(v1, v2, screen_width, screene_height, rasterized_points);
		rasterize_wire_aux(v0, v2, screen_width, screene_height, rasterized_points);
	}

	static bool insideTriangle(float x, float y, const glm::ivec2& v0,const glm::ivec2& v1,const glm::ivec2& v2) 
	{	
		glm::vec2 p = glm::vec2(x, y);
		glm::vec2 ab = v1 - v0;
		glm::vec2 bc = v2 - v1;
		glm::vec2 ca = v0 - v2;

		glm::vec2 ap = p - glm::vec2(v0);
		glm::vec2 bp = p - glm::vec2(v1);
		glm::vec2 cp = p - glm::vec2(v2);

		if( ab.x*ap.y-ab.y*ap.x > 0 && bc.x*bp.y-bc.y*bp.x > 0 && ca.x*cp.y-ca.y*cp.x > 0)
			return true;
		if (ab.x * ap.y - ab.y * ap.x < 0 && bc.x * bp.y - bc.y * bp.x < 0 && ca.x * cp.y - ca.y * cp.x < 0)
			return true;
		return false;
	}

	glm::vec3 barycentric(float x, float y, const glm::ivec2& v0,const glm::ivec2& v1,const glm::ivec2& v2) {
		glm::vec3 s1 = glm::vec3(v1.x - v0.x, v2.x - v0.x, v0.x - x);
		glm::vec3 s2 = glm::vec3(v1.y - v0.y, v2.y - v0.y, v0.y - y);
		glm::vec3 u = glm::cross(s1, s2);
		if (std::abs(u.z)> 1e-2) {
			return glm::vec3(1.f - (u.x + u.y) / u.z, u.x / u.z, u.y / u.z);
		}
		else {
			return glm::vec3(-1.f, 1.f, 1.f);
		}
	}

	void TRShaderPipeline::rasterize_fill_edge_function(
		const VertexData &v0,
		const VertexData &v1,
		const VertexData &v2,
		const unsigned int &screen_width,
		const unsigned int &screen_height,
		std::vector<VertexData> &rasterized_points)
	{
		//Edge-function rasterization algorithm

		//Task4: Implement edge-function triangle rassterization algorithm
		// Note: You should use VertexData::barycentricLerp(v0, v1, v2, w) for interpolation, 
		//       interpolated points should be pushed back to rasterized_points.
		//       Interpolated points shold be discarded if they are outside the window. 

		//       v0.spos, v1.spos and v2.spos are the screen space vertices.

		//For instance:
		int leftb = std::min(v0.spos.x, std::min(v1.spos.x, v2.spos.x));
		int rightb = std::max(v0.spos.x, std::max(v1.spos.x, v2.spos.x));
		int lowerb = std::min(v0.spos.y, std::min(v1.spos.y, v2.spos.y));
		int upperb = std::max(v0.spos.y, std::max(v1.spos.y, v2.spos.y));

		std::vector < std::vector<float>> pos{
			{0.25,0.25},{0.75,0.25},{0.25,0.75},{0.75,0.75}
		};

		for (int x = leftb; x <= rightb; ++x) {
			for (int y = lowerb; y <= upperb; ++y) {
				for (int i = 0; i < 4; ++i) {
					if (insideTriangle((float)x + pos[i][0], (float)y + pos[i][1], v0.spos, v1.spos, v2.spos)) {
						auto w = barycentric((float)x + pos[i][0], (float)y + pos[i][1], v0.spos, v1.spos, v2.spos);
						auto v = VertexData::barycentricLerp(v0, v1, v2, w);
						if (v.spos.x >= 0 && v.spos.x <= screen_width && v.spos.y >= 0 && v.spos.y <= screen_height)
						{
							rasterized_points.push_back(v);
						}
					}
				}
				//if (insideTriangle((float)x + 0.5, (float)y + 0.5, v0.spos, v1.spos, v2.spos)) {
				//	auto w = barycentric((float)x + 0.5, (float)y + 0.5, v0.spos, v1.spos, v2.spos);
				//	auto v = VertexData::barycentricLerp(v0, v1, v2, w);
				//	if (v.spos.x >= 0 && v.spos.x <= screen_width && v.spos.y >= 0 && v.spos.y <= screen_height)
				//	{
				//		rasterized_points.push_back(v);
				//	}
				//}
			}
		}

	}

	void TRShaderPipeline::rasterize_wire_aux(
		const VertexData &from,
		const VertexData &to,
		const unsigned int &screen_width,
		const unsigned int &screen_height,
		std::vector<VertexData> &rasterized_points)
	{

		//Task1: Implement Bresenham line rasterization
		// Note: You shold use VertexData::lerp(from, to, weight) for interpolation,
		//       interpolated points should be pushed back to rasterized_points.
		//       Interpolated points shold be discarded if they are outside the window. 
		
		//       from.spos and to.spos are the screen space vertices.

		//For instance:
		int deltax = to.spos.x - from.spos.x;
		int deltay = to.spos.y - from.spos.y;
		int xstep = 1, ystep = 1;
		if (deltax < 0) {
			xstep = -1;
			deltax = -deltax;
		}
		if (deltay < 0) {
			ystep = -1;
			deltay = -deltay;
		}
		int dy2_dx2 = deltay * 2 - deltax * 2, dx2 = 2 * deltax, dy2 = 2 * deltay;
		int pi;
		int sx = from.spos.x, sy = from.spos.y;
		rasterized_points.push_back(from);
		
		if (deltay <= deltax) {
			pi = dy2 - deltax;
			for (int i = 0; i <= deltax; ++i) {
				auto tmp = VertexData::lerp(from, to, (float)i / deltax);
				tmp.spos = glm::ivec2(sx, sy);
				if (tmp.spos.x >= 0 && tmp.spos.x <= screen_width && tmp.spos.y >= 0 && tmp.spos.y <= screen_height) {
					rasterized_points.push_back(tmp);
				}
				sx += xstep;
				if (pi <= 0)	pi += dy2;
				else {
					pi += dy2_dx2;
					sy += ystep;
				}
			}
		}
		else {
			pi = dx2 - deltay;
			for (int i = 0; i <= deltay; ++i) {
				auto tmp = VertexData::lerp(from, to, (float)i / deltay);
				tmp.spos = glm::ivec2(sx, sy);
				if (tmp.spos.x >= 0 && tmp.spos.x <= screen_width && tmp.spos.y >= 0 && tmp.spos.y <= screen_height) {
					rasterized_points.push_back(tmp);
				}
				sy += ystep;
				if (pi <= 0)	pi += dx2;
				else {
					pi -= dy2_dx2;
					sx += xstep;
				}
			}
		}
		rasterized_points.push_back(to);
	}

	void TRDefaultShaderPipeline::vertexShader(VertexData &vertex)
	{
		//Local space -> World space -> Camera space -> Project space
		vertex.pos = m_model_matrix * glm::vec4(vertex.pos.x, vertex.pos.y, vertex.pos.z, 1.0f);
		vertex.cpos = m_view_project_matrix * vertex.pos;
	}

	void TRDefaultShaderPipeline::fragmentShader(const VertexData &data, glm::vec4 &fragColor)
	{
		//Just return the color.
		fragColor = glm::vec4(data.tex, 0.0, 1.0f);
	}
}