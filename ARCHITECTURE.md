# OpenC3 Developer Toolkit — 아키텍처 설명서

> C++20 · Qt 6 · CMake · MSVC 2022 · Windows 11

---

## 1. 전체 구조 (레이어드 아키텍처)

```
┌─────────────────────────────────────────────────┐
│                  app/                           │  ← 진입점 · DI 조립
├─────────────────────────────────────────────────┤
│              ui/ (Qt Widgets)                   │  ← 화면 표시만
├─────────────────────────────────────────────────┤
│           viewmodels/ (Qt + 비즈니스 연결)       │  ← MVVM 브릿지
├─────────────────────────────────────────────────┤
│         services/ (순수 C++, Qt 없음)            │  ← 비즈니스 로직
├─────────────────────────────────────────────────┤
│          core/ (순수 C++20, Qt 없음)             │  ← 플랫폼 추상화
├─────────────────────────────────────────────────┤
│        models/ (헤더 전용, 의존성 없음)           │  ← 도메인 데이터
└─────────────────────────────────────────────────┘
```

**의존성 방향 규칙 (절대 역방향 불가)**
```
app → ui → viewmodels → services → core
                    ↘              ↗
                      models (공용)
```

- 아래 레이어는 위 레이어를 절대 참조하지 않습니다.
- Qt는 `viewmodels` 레이어부터 등장합니다. `services`와 `core`에는 Qt가 없습니다.

---

## 2. 각 레이어 상세

### 2-1. `models/` — 도메인 데이터 (헤더 전용)

순수 C++ 값 타입(struct)만 모아 둔 레이어입니다.
어떤 외부 라이브러리에도 의존하지 않습니다.

| 파일 | 내용 |
|------|------|
| `ConnectionProfile.h` | WSL/SSH 연결 설정 (호스트, 포트, 인증, COSMOS 경로) |
| `DockerContainer.h` | 컨테이너 상태, 포트, 통계 |
| `SystemMetrics.h` | CPU / 메모리 / 디스크 사용량 |
| `HealthCheckResult.h` | 헬스체크 결과 (PASS / WARNING / FAIL) |
| `Plugin.h` | 플러그인 메타데이터, 설치 상태 |

**왜 별도 레이어인가?**
모든 레이어가 이 타입들을 공유해야 하는데, 어느 한 레이어에 넣으면
아래 레이어가 위 레이어를 참조하는 순환 의존이 생깁니다.
헤더 전용 인터페이스 라이브러리로 분리하면 누구든 `#include` 가능합니다.

---

### 2-2. `core/` — 플랫폼 추상화 (순수 C++20)

운영체제 및 외부 시스템과의 저수준 통신을 담당합니다.
Qt가 전혀 없어서 단위 테스트가 쉽습니다.

#### 핵심 인터페이스
```cpp
// core/connection/ICommandExecutor.h
class ICommandExecutor {
public:
    virtual bool        connect()                              = 0;
    virtual void        disconnect()                           = 0;
    virtual std::string execute(const std::string& cmd)       = 0;
    virtual bool        writeFile(const std::string& path,
                                  const std::string& data)    = 0;
    virtual std::string readFile(const std::string& path)     = 0;
    // ... 스트리밍, 디렉터리 목록 등
};
```

#### 구현체

| 클래스 | 설명 |
|--------|------|
| `WslExecutor` | `wsl.exe`를 통해 Windows에서 Linux 명령 실행 |
| `SshExecutor` | libssh2를 이용한 SSH 원격 실행 |
| `NullCommandExecutor` | 연결 전 안전한 기본값 (Null Object 패턴) |
| `ExecutorProxy` | 연결/해제 시 실제 Executor를 원자적으로 교체 |

#### ExecutorProxy 패턴 (중요)

```
서비스들 ──참조──→ ExecutorProxy (불변 주소)
                         │
                   원자적 포인터 교체
                    /           \
             [연결 후]       [해제 후]
          SshExecutor    NullExecutor
```

서비스들은 생성 시점에 `ExecutorProxy&`를 받아 저장합니다.
이후 연결/해제가 일어나도 서비스는 같은 주소를 계속 사용하고,
`ExecutorProxy` 내부의 포인터만 원자적(`std::atomic`)으로 바뀝니다.
스레드 경쟁 조건 없이 안전하게 Executor를 교체할 수 있습니다.

---

### 2-3. `services/` — 비즈니스 로직 (순수 C++, Qt 없음)

"무엇을 해야 하는가"를 정의하는 레이어입니다.
인터페이스(`I*.h`)와 구현(`*Service.h/cpp`)을 분리합니다.

#### 제공 서비스

| 인터페이스 | 구현 | 역할 |
|------------|------|------|
| `IConnectionService` | `ConnectionService` | 연결 프로파일 관리, 상태 머신 |
| `IDockerService` | `DockerService` | 컨테이너 목록/제어, Docker CLI 파싱 |
| `ISystemService` | `SystemService` | CPU/메모리/디스크 메트릭 수집 |
| `IDoctorService` | `DoctorService` | 11가지 헬스체크 실행 |
| `IPluginService` | `PluginService` | gem 설치/제거/검증 |
| `ISettingsService` | `SettingsService` | 프로파일 JSON 직렬화/저장 |
| `IRemoteFileService` | `RemoteFileService` | 원격 파일 읽기/쓰기/목록 |

#### 인터페이스-구현 분리의 이유

```cpp
// 서비스가 의존하는 것은 인터페이스뿐
class DoctorService {
public:
    explicit DoctorService(ICommandExecutor& exec);
    // ...
};
```

- 단위 테스트 시 `MockCommandExecutor`로 교체 가능
- 나중에 SSH → 다른 프로토콜로 바꿔도 DoctorService 수정 불필요

---

### 2-4. `viewmodels/` — MVVM 브릿지 (Qt 첫 등장)

UI와 비즈니스 로직 사이를 연결하는 레이어입니다.
Qt의 `QObject`, 시그널/슬롯, `QtConcurrent` 스레딩이 여기에 집중됩니다.

#### 역할
1. **서비스를 비동기 호출** — `QtConcurrent::run`으로 워커 스레드에서 실행
2. **결과를 GUI 스레드로 마샬링** — `QMetaObject::invokeMethod(Qt::QueuedConnection)`
3. **시그널로 UI에 알림** — `emit statusChanged()`, `emit dataLoaded()` 등
4. **상태 캐시** — `busy_`, `connected_` 등 UI가 즉시 읽을 수 있는 값

#### 스레딩 패턴 (공통)

```cpp
// 모든 ViewModel이 사용하는 패턴
void SomeViewModel::loadData() {
    setBusy(true);                          // GUI 스레드
    (void)QtConcurrent::run([this] {
        // ──── 워커 스레드 ────
        auto result = service_.fetchData();
        // ────────────────────
        QMetaObject::invokeMethod(this, [this, result] {
            emit dataLoaded(result);        // GUI 스레드로 복귀
            setBusy(false);
        }, Qt::QueuedConnection);
    });
}
```

#### ViewModel 목록

| ViewModel | 대응 서비스 | 책임 |
|-----------|-------------|------|
| `DashboardViewModel` | Connection + Docker + System | 메트릭 폴링 (QTimer) |
| `DockerViewModel` | Docker | 컨테이너 테이블 모델 + 제어 |
| `DoctorViewModel` | Doctor | 헬스체크 테이블 모델 + 진행률 |
| `SettingsViewModel` | Settings + Connection | 프로파일 CRUD + 연결 상태 |
| `PluginViewModel` | Plugin | 플러그인 테이블 모델 + 설치/제거 |
| `CmdTlmViewModel` | RemoteFile | 파일 브라우즈/읽기/쓰기/파싱 |
| `InfraViewModel` | Connection + RemoteFile | ENV/Compose 편집, 볼륨 오버라이드, 플러그인 스캐폴딩 |
| `PacketToolsViewModel` | RemoteFile | 패킷 로그 파일 분석 |
| `LogViewerViewModel` | RemoteFile | 실시간 로그 스트리밍 |

#### 유틸리티 클래스 (ViewModel 레이어 내)

| 클래스 | 역할 |
|--------|------|
| `CmdTlmParser` | COSMOS `.txt` 파일 파싱 (블록/아이템/진단 결과 반환) |
| `PluginTemplateEngine` | 플러그인/타겟 파일 템플릿 생성 (순수 함수, 원격 I/O 없음) |

> **`PluginTemplateEngine`이 ViewModel 레이어에 있는 이유:**
> `QString`, `QMap` 등 Qt 타입을 사용하기 때문에 `services/core` 레이어에 넣을 수 없습니다.
> 그러나 어떤 상태도 없는 순수 정적 메서드이므로 비즈니스 로직을 ViewModel에 넣지 않는다는 원칙에 위배되지 않습니다.

---

### 2-5. `ui/` — 표현 레이어 (Qt Widgets)

"어떻게 보여줄 것인가"만 담당합니다.
ViewModel에 의존하고, 비즈니스 로직을 직접 실행하지 않습니다.

#### 구조

```
ui/
├── MainWindow         — 내비게이션 레일 + QStackedWidget
├── views/             — 전체 화면 모듈 (8개)
│   ├── DashboardView
│   ├── DockerView
│   ├── InfraView
│   ├── DoctorView
│   ├── PluginView
│   ├── CmdTlmView
│   ├── PacketToolsView
│   ├── LogViewerView
│   └── SettingsView
├── dialogs/           — 팝업 다이얼로그
│   ├── ConnectionDialog   — 시작 시 연결 선택
│   ├── PluginWizard       — 3단계 플러그인 생성 위자드
│   ├── AddTargetDialog    — 기존 플러그인에 타겟 추가
│   ├── UserGuideDialog    — 내장 사용자 가이드
│   └── AboutDialog
├── widgets/           — 재사용 컴포넌트
│   ├── StatusBadge        — 색상 상태 뱃지
│   ├── MetricCard         — 메트릭 표시 카드
│   ├── LogWidget          — 스크롤 로그 뷰어
│   └── CmdTlmHighlighter  — COSMOS .txt 문법 강조
└── styles/
    └── ThemeManager       — 다크 테마 QSS 적용
```

#### View의 의무와 금지사항

✅ 해야 하는 것
- ViewModel의 시그널을 받아 화면 업데이트
- 사용자 입력을 ViewModel의 슬롯으로 전달
- 레이아웃, 애니메이션, 스타일링

❌ 하면 안 되는 것
- 서비스 직접 호출
- 비즈니스 규칙 구현
- 데이터 변환/파싱

---

### 2-6. `app/` — 조립(Composition Root)

의존성 주입(DI)의 진입점입니다.
여기서만 모든 레이어의 구체 타입을 알고 있습니다.

```
Application::run()
  ├─ registerServices()      — 구체 서비스 생성 + ServiceRegistry 등록
  ├─ registerViewModels()    — ViewModel 생성 (서비스 주입)
  ├─ ConnectionDialog.exec() — 시작 연결
  └─ MainWindow.show()       — 메인 창 표시 + 이벤트 루프 시작
```

`ServiceRegistry`는 타입 인덱스 기반 DI 컨테이너입니다.
```cpp
registry_.registerInstance<IDockerService>(dockerSvc);
auto docker = registry_.resolve<IDockerService>(); // shared_ptr 반환
```

---

## 3. 핵심 패턴 정리

### 패턴 1: Null Object (NullCommandExecutor)

연결 전에도 서비스가 안전하게 동작하도록 합니다.
```cpp
// 연결 전: NullCommandExecutor::execute() → 빈 문자열 반환
// 연결 후: SshExecutor::execute() → 실제 SSH 실행
// 서비스는 이 차이를 신경 쓰지 않음
```

### 패턴 2: Strategy (ICommandExecutor)

```
ICommandExecutor
    ├── WslExecutor   (WSL 모드)
    ├── SshExecutor   (SSH 모드)
    └── NullCommandExecutor (연결 전)
```
런타임에 전략을 교체해도 서비스 코드 변경 없음.

### 패턴 3: Proxy (ExecutorProxy)

서비스는 `ExecutorProxy&`를 받아 저장합니다.
연결/해제 시 `ExecutorProxy` 내부 포인터만 교체되고,
서비스가 저장한 참조 주소는 변하지 않습니다.
`std::atomic`으로 스레드 안전성을 보장합니다.

### 패턴 4: Observer (onStateChanged 콜백)

```cpp
// 서비스 레이어: C++ 콜백 (Qt 없음)
connection_.onStateChanged([this](const ConnectionEvent& ev) {
    // ...
});

// ViewModel 레이어: Qt 시그널로 변환
QMetaObject::invokeMethod(this, [this, s] {
    emit connectionStateChanged(s); // GUI 스레드에서 발생
}, Qt::QueuedConnection);
```
서비스는 Qt를 몰라도 됩니다.
ViewModel이 C++ 콜백을 Qt 시그널로 번역합니다.

### 패턴 5: MVVM

```
View ─(시그널/슬롯)→ ViewModel ─(함수 호출)→ Service
View ←(Q_PROPERTY)── ViewModel ←(콜백)──── Service
```
- View는 데이터를 직접 갖지 않고 ViewModel만 바라봄
- ViewModel은 UI를 모르고 데이터와 커맨드만 노출
- Service는 Qt를 모름

---

## 4. 데이터 흐름 예시 — 도커 컨테이너 목록 조회

```
[사용자] DockerView에서 "Refresh" 버튼 클릭
    ↓
[DockerView] vm_.refresh() 호출  ← View가 ViewModel에 위임
    ↓
[DockerViewModel] QtConcurrent::run() 으로 워커 스레드 시작
    ↓ (워커 스레드)
[DockerService] executor_.execute("docker ps --format json") 실행
    ↓
[DockerService] JSON 파싱 → vector<DockerContainer> 반환
    ↓
[DockerViewModel] QMetaObject::invokeMethod(Qt::QueuedConnection)
    ↓ (GUI 스레드 복귀)
[ContainerTableModel] setContainers() 호출 → beginResetModel/endResetModel
    ↓
[DockerView] QTableView 자동 갱신 (Qt 모델/뷰 메커니즘)
    ↓
[사용자] 업데이트된 컨테이너 목록 확인
```

---

## 5. 연결 상태 머신

```
[시작]
  │
  ▼
Disconnected ──connect()──→ Connecting ──성공──→ Connected
     ▲                                              │
     │                        실패                  │
     └──────────────────── Error ◄──────────────────┘
                              │    disconnect()
                              └────────────────→ Disconnected
```

`ConnectionService::setState()`가 상태를 바꿀 때마다
등록된 모든 `onStateChanged` 콜백이 즉시 동기 호출됩니다.
ViewModel은 이 콜백을 Qt 시그널로 변환해 UI에 전달합니다.

---

## 6. 빌드 구조

```
src/
├── CMakeLists.txt          — 최상위 (레이어 순서 정의)
├── core/CMakeLists.txt     — opencosmos_core (spdlog, nlohmann_json, libssh2)
├── models/CMakeLists.txt   — opencosmos_models (INTERFACE, 헤더 전용)
├── services/CMakeLists.txt — opencosmos_services (core + models)
├── viewmodels/CMakeLists.txt — opencosmos_viewmodels (services + Qt6::Core/Concurrent)
├── ui/CMakeLists.txt       — opencosmos_ui (viewmodels + Qt6::Widgets/Network)
└── app/CMakeLists.txt      — OpenC3DevToolkit (실행 파일)
```

**빌드 명령**
```powershell
cmake --preset local-debug
cmake --build --preset local-debug
```

실행 파일: `build/local-debug/bin/OpenC3DevToolkit.exe`
Qt DLL은 `windeployqt`가 POST_BUILD 단계에서 자동 복사합니다.

---

## 7. 클린 코드 규칙 요약

| 규칙 | 적용 방법 |
|------|-----------|
| **단일 책임 (SRP)** | 각 클래스는 하나의 이유로만 변경됨. 예: `PluginTemplateEngine`은 템플릿 생성만 담당 |
| **의존성 역전 (DIP)** | 서비스는 구체 클래스가 아닌 인터페이스(`I*.h`)에 의존 |
| **인터페이스 분리 (ISP)** | 각 서비스 인터페이스는 자신의 도메인 메서드만 선언 |
| **개방/폐쇄 (OCP)** | 새 연결 타입 추가 = `ICommandExecutor` 구현체 추가, 기존 서비스 수정 없음 |
| **함수는 작게** | `setupUi()`처럼 긴 함수는 논리 블록별로 주석으로 구분 |
| **매직 스트링 최소화** | 연결 상태는 `ConnectionState` enum, 버튼 상태는 `isConnected()` 조회 |
| **RAII** | `std::unique_ptr`, Qt 부모 소유권으로 수명 관리 |
| **const 우선** | 변경되지 않는 변수/메서드에 `const`/`noexcept` 명시 |
| **워커 스레드 격리** | 모든 원격 I/O는 `QtConcurrent::run`으로 GUI 블로킹 방지 |
