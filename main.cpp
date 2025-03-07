
#pragma region Include
#include <windows.h>

#pragma region D3D11
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#include <d3d11.h>
#include <d3dcompiler.h>
#pragma endregion

#pragma region ImGui
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#pragma endregion

#include "Sphere.h"
#pragma endregion

#pragma region Structs
struct FVector3
{
	float x, y, z;

	FVector3(float x, float y, float z) : x(x), y(y), z(z) {}

	FVector3 operator+(const FVector3& other) const { return FVector3(x + other.x, y + other.y, z + other.z); }
	FVector3 operator-(const FVector3& other) const { return FVector3(x - other.x, y - other.y, z - other.z); }
	FVector3 operator*(float scalar) const { return FVector3(x * scalar, y * scalar, z * scalar); }
	FVector3 operator/(float scalar) const { return FVector3(x / scalar, y / scalar, z / scalar); }

	FVector3& operator+=(const FVector3& other) { x += other.x; y += other.y; z += other.z; return *this; }
	FVector3& operator-=(const FVector3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
	FVector3& operator*=(float scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
	FVector3& operator/=(float scalar) { x /= scalar; y /= scalar; z /= scalar; return *this; }

	float LengthSquared() const { return x * x + y * y + z * z; }
	float Length() const { return sqrt(LengthSquared()); }
};
#pragma endregion

#pragma region Renderer Class
class URenderer
{
	#pragma region Members
public:
	ID3D11Device* Device = nullptr;	// GPU와 통신하기 위한 Direct3D 장치
	ID3D11DeviceContext* DeviceContext = nullptr;	// GPU 명령 실행을 담당하는 컨텍스트
	IDXGISwapChain* SwapChain = nullptr;	// 프레임 버퍼 교체하는 데 사용되는 스왑 체인

	ID3D11Texture2D* FrameBuffer = nullptr;	// 화면 출력용 텍스처
	ID3D11RenderTargetView* FrameBufferRTV = nullptr;	// 텍스처를 렌더 타겟으로 사용하는 뷰
	ID3D11RasterizerState* RasterizerState = nullptr;	// 래스터라이저 상태 (컬링, 채우기 모드 등 정의)
	ID3D11Buffer* ConstantBuffer = nullptr;	// 상수 버퍼 (쉐이더에 데이터 전달)

	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };	// 화면 초기화 시 사용되는 색상
	D3D11_VIEWPORT ViewportInfo;	// 렌더링 영역을 정의하는 뷰포트 정보

	ID3D11VertexShader* SimpleVertexShader;
	ID3D11PixelShader* SimplePixelShader;
	ID3D11InputLayout* SimpleInputLayout;
	unsigned int Stride;

	struct FConstants
	{
		FVector3 Offset;
		float Radius;
	};
	#pragma endregion

	#pragma region Methods

	#pragma region Prepare
	void Prepare()
	{
		DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);

		DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		DeviceContext->RSSetViewports(1, &ViewportInfo);
		DeviceContext->RSSetState(RasterizerState);

		DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, nullptr);
		DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
	}

	void PrepareShader()
	{
		DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
		DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);
		DeviceContext->IASetInputLayout(SimpleInputLayout);

		if (ConstantBuffer)
		{
			DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
		}
	}
	#pragma endregion

	#pragma region Render Primitive
	// 생성한 버텍스 버퍼 GPU에 넘겨 실질적인 렌더링 요청
	void RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices)
	{
		UINT offset = 0;
		DeviceContext->IASetVertexBuffers(0, 1, &pBuffer, &Stride, &offset);
		DeviceContext->Draw(numVertices, 0);
	}
	#pragma endregion

	#pragma region Renderer Initialize & Release
	void Create(HWND hWindow)
	{
		CreateDeviceAndSwapChain(hWindow);
		CreateFrameBuffer();
		CreateRasterizerState();
	}
	void Release()
	{
		RasterizerState->Release();
		DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);   // 렌더 타겟 초기화

		ReleaseFrameBuffer();
		ReleaseDeviceAndSwapChain();
	}

	void CreateDeviceAndSwapChain(HWND hWindow)
	{
		// 지원하는 Direct3D 기능 레벨 정의
		D3D_FEATURE_LEVEL featurelevels[] = { D3D_FEATURE_LEVEL_11_0 };

		// SwapChain 설정 구조체 초기화
		DXGI_SWAP_CHAIN_DESC swapchaindesc = {};
		swapchaindesc.BufferDesc.Width = 0; // 창 크기에 맞게 자동으로 설정
		swapchaindesc.BufferDesc.Height = 0; // 창 크기에 맞게 자동으로 설정
		swapchaindesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // 색상 포맷
		swapchaindesc.SampleDesc.Count = 1; // 멀티 샘플링 비활성화
		swapchaindesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 백버퍼 사용 용도 설정 (렌더 타겟으로 사용)
		swapchaindesc.BufferCount = 2; // 백버퍼 개수(더블 버퍼링)
		swapchaindesc.OutputWindow = hWindow; // 버퍼를 출력할 윈도우
		swapchaindesc.Windowed = TRUE; // 윈도우 전체화면 모드
		swapchaindesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // 스왑 방식

		// Device, DeviceContext, SwapChain 생성 (Direct3D 환경 구축)
		D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
			D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
			featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION,
			&swapchaindesc, &SwapChain, &Device, nullptr, &DeviceContext);

		// 생성된 SwapChain 정보 가져오기
		SwapChain->GetDesc(&swapchaindesc);
		// Viewport 정보 설정
		ViewportInfo = { 0.0f, 0.0f, (float)swapchaindesc.BufferDesc.Width, (float)swapchaindesc.BufferDesc.Height, 0.0f, 1.0f };
	}
	void ReleaseDeviceAndSwapChain()
	{
		if (DeviceContext)
		{
			DeviceContext->Flush();	// 남아있는 GPU 명령 실행
		}

		if (SwapChain)
		{
			SwapChain->Release();
			SwapChain = nullptr;
		}

		if (Device)
		{
			Device->Release();
			Device = nullptr;
		}

		if (DeviceContext)
		{
			DeviceContext->Release();
			DeviceContext = nullptr;
		}
	}

	void CreateFrameBuffer()
	{
		// SwapChain으로부터 백 버퍼 텍스처 가져오기
		SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

		// 렌더 타겟 뷰 생성
		D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
		framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

		Device->CreateRenderTargetView(FrameBuffer, &framebufferRTVdesc, &FrameBufferRTV);
	}
	void ReleaseFrameBuffer()
	{
		if (FrameBuffer)
		{
			FrameBuffer->Release();
			FrameBuffer = nullptr;
		}

		if (FrameBufferRTV)
		{
			FrameBufferRTV->Release();
			FrameBufferRTV = nullptr;
		}
	}

	void CreateRasterizerState()
	{
		D3D11_RASTERIZER_DESC rasterizerdesc = {};
		rasterizerdesc.FillMode = D3D11_FILL_SOLID;
		rasterizerdesc.CullMode = D3D11_CULL_BACK;

		Device->CreateRasterizerState(&rasterizerdesc, &RasterizerState);
	}
	void ReleaseRasterizerState()
	{
		if (RasterizerState)
		{
			RasterizerState->Release();
			RasterizerState = nullptr;
		}
	}
	#pragma endregion

	#pragma region Shader
	void CreateShader()
	{
		// ID3DBlob : 컴파일된 셰이더 오브젝트(CSO) 저장하는 버퍼
		ID3DBlob* vertexshaderCSO;
		ID3DBlob* pixelshaderCSO;

		// Vertex Shader
		D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &vertexshaderCSO, nullptr);
		Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), nullptr, &SimpleVertexShader);

		// Pixel Shader
		D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &pixelshaderCSO, nullptr);
		Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(), pixelshaderCSO->GetBufferSize(), nullptr, &SimplePixelShader);

		// layout object
		D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		Device->CreateInputLayout(layout, ARRAYSIZE(layout), vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), &SimpleInputLayout);

		Stride = sizeof(FVertexSimple);

		vertexshaderCSO->Release();
		pixelshaderCSO->Release();
	}
	void ReleaseShader()
	{
		if (SimpleInputLayout)
		{
			SimpleInputLayout->Release();
			SimpleInputLayout = nullptr;
		}

		if (SimplePixelShader)
		{
			SimplePixelShader->Release();
			SimplePixelShader = nullptr;
		}

		if (SimpleVertexShader)
		{
			SimpleVertexShader->Release();
			SimpleVertexShader = nullptr;
		}
	}
	#pragma endregion

	#pragma region Buffer
	void SwapBuffer()
	{
		SwapChain->Present(1, 0);
	}

	ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth)
	{
		D3D11_BUFFER_DESC vertexbufferdesc = {};
		vertexbufferdesc.ByteWidth = byteWidth;
		vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexbufferdesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertices };

		ID3D11Buffer* vertexBuffer;

		Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

		return vertexBuffer;
	}
	void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer)
	{
		vertexBuffer->Release();
	}

	void CreateConstantBuffer()
	{
		D3D11_BUFFER_DESC constantbufferdesc = {};
		constantbufferdesc.ByteWidth = sizeof(FConstants) + 0xf & 0xfffffff0; 
		constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC; // will be updated from CPU every frame
		constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

		Device->CreateBuffer(&constantbufferdesc, nullptr, &ConstantBuffer);
	}
	void ReleaseConstantBuffer()
	{
		if (ConstantBuffer)
		{
			ConstantBuffer->Release();
			ConstantBuffer = nullptr;
		}
	}
	void UpdateConstantBuffer(FVector3 Offset, float Radius)
	{
		if (ConstantBuffer)
		{
			D3D11_MAPPED_SUBRESOURCE constantbufferMSR;

			DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);
			FConstants* constants = (FConstants*)constantbufferMSR.pData;
			{
				constants->Offset = Offset;
				constants->Radius = Radius;
			}
			DeviceContext->Unmap(ConstantBuffer, 0);
		}
	}
	#pragma endregion 

	#pragma endregion
};
#pragma endregion

#pragma region Ball Class
class UBall
{
public:
	#pragma region Members
	FVector3 Location;
	FVector3 Velocity;
	float Radius;
	float Mass;
	UBall* NextBall;
	#pragma endregion

	#pragma region Constructor & Destructor
	UBall(const FVector3 location, const FVector3 velocity, float radius)
		: Location(location), Velocity(velocity), Radius(radius)
	{
		CalculateMass();
	}

	~UBall(){}
	#pragma endregion

	#pragma region Method
	void CalculateMass()
	{
		Mass = 3.14159f * powf(Radius, 2) * 1;
	}

	void Render(URenderer* renderer, ID3D11Buffer* vertexBufferSphere, UINT numVerticesSphere)
	{
		renderer->UpdateConstantBuffer(Location, Radius);
		renderer->RenderPrimitive(vertexBufferSphere, numVerticesSphere);
	}

	void Move()
	{
		Location += Velocity;
	}
	#pragma endregion
};

class BallLinkedList
{
private:
	UBall* Head;
	int Size;

public:
	BallLinkedList() : Head(nullptr), Size(0) {}

	~BallLinkedList()
	{
		UBall* current = Head;
		while (current != nullptr)
		{
			UBall* next = current->NextBall;
			delete current;
			current = next;
		}
		Head = nullptr;
		Size = 0;
	}

	void AddBall(UBall* ball)
	{
		if (!ball) return;

		ball->NextBall = Head;
		Head = ball;
		Size++;
	}

	bool RemoveBall(int index)
	{
		if (!Head || index < 0 || index >= Size) {
			return false;
		}

		UBall* current = Head;
		UBall* previous = nullptr;

		for (int i = 0; i < index; i++) {
			previous = current;
			current = current->NextBall;
			if (current == nullptr)
			{
				return false;
			}
		}

		if (previous == nullptr)
		{
			Head = current->NextBall;
		}
		else
		{
			previous->NextBall = current->NextBall;
		}

		delete current;
		current = nullptr;
		Size--;

		return true;
	}

	bool RemoveRandomBall()
	{
		if (IsEmpty())
		{
			return false;
		}

		int randomIndex = rand() % Size;
		return RemoveBall(randomIndex);
	}

	bool IsEmpty() const
	{
		return Head == nullptr;
	}

	int GetSize() const
	{
		return Size;
	}

	UBall* GetHead() const
	{
		return Head;
	}
};
#pragma endregion

#pragma region Window Callback
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
	{
		return true;
	}

	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
#pragma endregion

#pragma region Main Window
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	#pragma region Window
	WCHAR WindowClass[] = L"JungleWindowClass";
	WCHAR Title[] = L"Game Tech Lab";
	WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };
	RegisterClassW(&wndclass);
	HWND hWnd = CreateWindowExW(0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1024, 1024,
		nullptr, nullptr, hInstance, nullptr);
	#pragma endregion

	#pragma region Create Renderer 
	URenderer renderer;
	renderer.Create(hWnd);
	renderer.CreateShader();
	renderer.CreateConstantBuffer();
	#pragma endregion

	#pragma region Create ImGui & Initialize
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplWin32_Init((void*)hWnd);
	ImGui_ImplDX11_Init(renderer.Device, renderer.DeviceContext);
	#pragma endregion

	#pragma region Create Vertex Buffer
	UINT numVerticesSphere = sizeof(sphere_vertices) / sizeof(FVertexSimple);
	ID3D11Buffer* vertexBufferSphere = renderer.CreateVertexBuffer(sphere_vertices, sizeof(sphere_vertices));
	#pragma endregion

	#pragma region Members
	bool bIsExit = false;

	const float leftBorder = -1.0f;
	const float rightBorder = 1.0f;
	const float topBorder = 1.0f;
	const float bottomBorder = -1.0f;

	bool bGravity = true;
	const float ballSpeed = .01f;
	const float gravity = -.005f;
	int numBalls = 1;
	const int minBalls = 1;
	BallLinkedList ballList;

	const int targetFPS = 30;
	const double targetFrameTime = 1000 / targetFPS;
	LARGE_INTEGER frequency;

	LARGE_INTEGER startTime, endTime;
	double elapsedTime = 0.0;
	#pragma endregion

	#pragma region Main

	QueryPerformanceFrequency(&frequency);
	while (bIsExit == false)
	{
		QueryPerformanceCounter(&startTime);
		
		#pragma region Window Callback
		MSG msg;
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);	
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
			{
				bIsExit = true;
				break;
			}
		}
		#pragma endregion

		#pragma region Update Ball
		if (ballList.GetSize() != numBalls)
		{
			if (numBalls > ballList.GetSize())
			{
				for (int i = ballList.GetSize(); i < numBalls; i++)
				{
					FVector3 location = {
						(float)(rand() % 150 - 100) / 100.0f,
						.5f,
						.0f };
					FVector3 velocity = {
							((float)(rand() % 150 - 50) / 100.0f) * ballSpeed,
							((float)(rand() % 150 - 50) / 100.0f) * ballSpeed,
							0.0f };
					float radius = (float)(rand() % 7 + 4) / 100.0f;

					UBall* newBall = new UBall(location, velocity, radius);
					ballList.AddBall(newBall);
				}
			}
			if (numBalls < ballList.GetSize())
			{
				int removBallCount = ballList.GetSize() - numBalls;
				for (int i = 0; i < removBallCount; i++)
				{
					ballList.RemoveRandomBall();
				}
			}
		}

		UBall* current = ballList.GetHead();
		while (current != nullptr)
		{
			if (bGravity)
				current->Velocity.y += gravity;

			#pragma region Ball Collision
			UBall* other = current->NextBall;
			while (other != nullptr)
			{
				FVector3 diff = current->Location - other->Location;
				float distSquared = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
				float radiusSum = current->Radius + other->Radius;

				if (distSquared <= radiusSum * radiusSum)
				{
					FVector3 normal = diff / sqrt(distSquared);
					FVector3 relativeVelocity = current->Velocity - other->Velocity;
					float velocityAlongNormal =
						(relativeVelocity.x * normal.x) +
						(relativeVelocity.y * normal.y) +
						(relativeVelocity.z * normal.z);

					if (velocityAlongNormal > 0)
					{
						other = other->NextBall;
						continue;
					}

					float restitution = 1.0f;
					float m1 = current->Mass;
					float m2 = other->Mass;
					float impulseScalar = -(1 + restitution) * velocityAlongNormal / (1 / m1 + 1 / m2);

					current->Velocity += normal * (impulseScalar / m1);
					other->Velocity -= normal * (impulseScalar / m2);

					float penetrationDepth = radiusSum - sqrt(distSquared);
					if (penetrationDepth > 0)
					{
						FVector3 correction = normal * (penetrationDepth / 2);
						current->Location += correction;
						other->Location -= correction;
					}
				}
				other = other->NextBall;
			}
			#pragma endregion

			current->Move();

			#pragma region Wall Collision
			float renderRadius = current->Radius;
			if (current->Location.x < leftBorder + renderRadius)
			{
				current->Location.x = leftBorder + renderRadius;
				current->Velocity.x *= -1.0f;
			}
			if (current->Location.x > rightBorder - renderRadius)
			{
				current->Location.x = rightBorder - renderRadius;
				current->Velocity.x *= -1.0f;
			}
			if (current->Location.y > topBorder - renderRadius)
			{
				current->Location.y = topBorder - renderRadius;
				current->Velocity.y *= -1.0f;
			}
			if (current->Location.y < bottomBorder + renderRadius)
			{
				current->Location.y = bottomBorder + renderRadius;
				current->Velocity.y *= -1.0f;
			}
			#pragma endregion

			current = current->NextBall;
		}
		#pragma endregion

		renderer.Prepare();
		renderer.PrepareShader();
		current = ballList.GetHead();
		while (current != nullptr)
		{
			current->Render(&renderer, vertexBufferSphere, numVerticesSphere);
			current = current->NextBall;
		}

		#pragma region ImGui
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Jungle Property Window");
		ImGui::Text("Hello Jungle World!");
		ImGui::Checkbox("Gravity", &bGravity);
		ImGui::InputInt("Number of Balls", &numBalls, 1);
		numBalls = max(1, numBalls);
		ImGui::End();

		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		#pragma endregion
		 
		renderer.SwapBuffer();
		do
		{
			Sleep(0);
			QueryPerformanceCounter(&endTime);
			elapsedTime = (endTime.QuadPart - startTime.QuadPart) * 1000 / frequency.QuadPart;

		} while (elapsedTime < targetFrameTime);
	}
	#pragma endregion

	#pragma region Release
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	renderer.ReleaseConstantBuffer();
	renderer.ReleaseShader();
	renderer.Release();
	#pragma endregion

	return 0;
}
#pragma endregion 