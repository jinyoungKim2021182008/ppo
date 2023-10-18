
#include "d3dUtil.h"
#include <comdef.h>
#include <fstream>

using Microsoft::WRL::ComPtr;

DxException::DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) :
    ErrorCode(hr),
    FunctionName(functionName),
    Filename(filename),
    LineNumber(lineNumber)
{
}

bool d3dUtil::IsKeyDown(int vkeyCode)
{
    return (GetAsyncKeyState(vkeyCode) & 0x8000) != 0;
}

// �����ϵ� ���̴� ����Ʈ�ڵ带 C++ ǥ�� ���� ����� ���̺귯���� �̿��� �����ϴ� �Լ�
// ��뿹��)
// ComPtr<ID3DBlob> mvsByteCode = d3dUtil::LoadBinary(L"Shaders\\color_cs.cos");
ComPtr<ID3DBlob> d3dUtil::LoadBinary(const std::wstring& filename)
{
    std::ifstream fin(filename, std::ios::binary);

    fin.seekg(0, std::ios_base::end);
    std::ifstream::pos_type size = (int)fin.tellg();
    fin.seekg(0, std::ios_base::beg);

    ComPtr<ID3DBlob> blob;
    ThrowIfFailed(D3DCreateBlob(size, blob.GetAddressOf()));

    fin.read((char*)blob->GetBufferPointer(), size);
    fin.close();

    return blob;
}

// ���� ���ϱ���(�����Ӹ��� ������ �ʴ� ���ϱ���)�� �׸� ���� ������ ������ ���� ���� ���۵��� Default Heap�� �ִ´�.
// ���� ���ϱ����� ��� �ʱ�ȭ�Ŀ��� GPU�� ������ �����Ƿ� Default Heap�� �ִ� ���� �մ��ϴ�.
// CPU�� �⺻���� �������۸� �������� ���Ѵ�.
// ����, D3D12_HEAP_TYPE_UPLOAD������ �ӽ� ���ε� ���۸� �����Ͽ� �ý��� �޸𸮿� �ִ� �����ڷḦ ���ε� ���ۿ� �����ϰ�, 
// ���ε� ������ �ڷḦ ���� �������۷� �����Ѵ�.
Microsoft::WRL::ComPtr<ID3D12Resource> d3dUtil::CreateDefaultBuffer(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    const void* initData,
    UINT64 byteSize,
    Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
{
    ComPtr<ID3D12Resource> defaultBuffer;

    // ���� �⺻ ���� �ڿ��� �����Ѵ�.
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

    // CPU�� �޸��� �ڷḦ �⺻ ���ۿ� �����ϱ� ����
    // �ӽ� ���ε� ���� �����Ѵ�.
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadBuffer.GetAddressOf())));


    // �⺻ ���ۿ� ������ �ڷḦ �����Ѵ�.
    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = initData;
    subResourceData.RowPitch = byteSize;
    subResourceData.SlicePitch = byteSize;

    // ���ε� ���ۿ��� �⺻ ���� �ڿ����� �ڷ� ���縦 ��û�Ѵ�.
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON, 
        D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources<1>(cmdList, 
        defaultBuffer.Get(), uploadBuffer.Get(), 
        0, 0, 1, &subResourceData);
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, 
        D3D12_RESOURCE_STATE_GENERIC_READ));

    // ���� �Լ� ȣ�� ���Ŀ��� uploadBuffer�� ��� �����ؾ� �Ѵ�.
    // ������ ���縦 �����ϴ� ���ɸ���� ���� ������� �ʾұ� �����̴�.
    // ���簡 �Ϸ�Ǿ����� Ȯ������ �Ŀ� uploadBuffer�� �����ϸ� �ȴ�.

    return defaultBuffer;
}

// 
ComPtr<ID3DBlob> d3dUtil::CompileShader(
    const std::wstring& filename,
    const D3D_SHADER_MACRO* defines,
    const std::string& entrypoint,
    const std::string& target)
{
    // ����� ��忡�� ����� ���� �÷��׵��� ����Ѵ�.
    UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = S_OK;

    ComPtr<ID3DBlob> byteCode = nullptr;
    ComPtr<ID3DBlob> errors;
    hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

    // ���� �޼����� ����� â�� ���
    if (errors != nullptr)
        OutputDebugStringA((char*)errors->GetBufferPointer());

    ThrowIfFailed(hr);

    return byteCode;
}

std::wstring DxException::ToString()const
{
    // ���� �ڵ��� ���ڿ� ������ �����ɴϴ�
    _com_error err(ErrorCode);
    std::wstring msg = err.ErrorMessage();

    return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
}

