#pragma once

#include "DnmGL/D3D12/Context.hpp"
#include <dxcapi.h>

namespace DnmGL::D3D12 {
    class Shader final : public DnmGL::Shader {
    public:
        Shader(D3D12::Context& context, std::string_view path);
        ~Shader() {}

        IDxcBlob *GetShaderBlob(std::string_view entry_point) const;
    private:
        std::map<std::string, ComPtr<IDxcBlob>> m_shaders;
    };

    inline IDxcBlob *Shader::GetShaderBlob(std::string_view entry_point) const {
        const auto it = m_shaders.find(std::string(entry_point));
        return it != m_shaders.end() ? it->second.Get() : nullptr;
    }
}