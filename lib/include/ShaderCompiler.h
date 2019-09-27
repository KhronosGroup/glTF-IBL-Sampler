#pragma once
#include <vector>

namespace IBLLib
{
	class ShaderCompiler
	{
	public:
		enum class Stage {
			Vertex,
			TessControl,
			TessEvaluation,
			Geometry,
			Fragment,
			Compute,
		};

		static ShaderCompiler& instance() { static ShaderCompiler inst; return inst; }

		bool compile(const std::vector<char>& _glslBlob, const char* _entryPoint, Stage _stage, std::vector<uint32_t>& _outSpvBlob);

	private:

		ShaderCompiler();
		~ShaderCompiler();
	};
}
