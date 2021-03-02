// OmahaUpdateTickle.cpp : This file contains the 'main' function. Program
// execution begins and ends there.
//

#include <comutil.h>
#include <windows.h>
#include <wrl/client.h>
#include <iostream>

#import "omaha3_idl.tlb"

//  CLSID_GoogleUpdate3WebMachineClass =
//  DECLSPEC_UUID("492e1c30-a1a2-4695-87c8-7a8cad6f936f")
enum CurrentState {
  STATE_INIT = 1,
  STATE_WAITING_TO_CHECK_FOR_UPDATE = 2,
  STATE_CHECKING_FOR_UPDATE = 3,
  STATE_UPDATE_AVAILABLE = 4,
  STATE_WAITING_TO_DOWNLOAD = 5,
  STATE_RETRYING_DOWNLOAD = 6,
  STATE_DOWNLOADING = 7,
  STATE_DOWNLOAD_COMPLETE = 8,
  STATE_EXTRACTING = 9,
  STATE_APPLYING_DIFFERENTIAL_PATCH = 10,
  STATE_READY_TO_INSTALL = 11,
  STATE_WAITING_TO_INSTALL = 12,
  STATE_INSTALLING = 13,
  STATE_INSTALL_COMPLETE = 14,
  STATE_PAUSED = 15,
  STATE_NO_UPDATE = 16,
  STATE_ERROR = 17
};

class COInitializeHolder {
 public:
  COInitializeHolder() : inited_(false) {
    HRESULT hr = ::CoInitialize(nullptr);
    if (FAILED(hr)) {
    }
    inited_ = true;
  }

  ~COInitializeHolder() {
    if (inited_) {
      ::CoUninitialize();
    }
  }

  operator bool() const { return inited_; }

 private:
  bool inited_;
};

// Explicitly allow the Google Update service to impersonate the client since
// some COM code elsewhere in the browser process may have previously used
// CoInitializeSecurity to set the impersonation level to something other than
// the default. Ignore errors since an attempt to use Google Update may succeed
// regardless.
HRESULT ConfigureProxyBlanket(IUnknown* interface_pointer) {
  return ::CoSetProxyBlanket(
      interface_pointer, RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT,
      COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_DYNAMIC_CLOAKING);
}

bool GetCurrentState(
    GoogleUpdate3Lib::IAppWeb* app,
    CurrentState* state_value,
    HRESULT* hresult) {
  Microsoft::WRL::ComPtr<IDispatch> dispatch;
  *hresult = app->get_currentState(&dispatch);
  if (FAILED(*hresult))
    return false;
  Microsoft::WRL::ComPtr<GoogleUpdate3Lib::ICurrentState> current_state;
  *hresult = dispatch.As(&(current_state));
  if (FAILED(*hresult))
    return false;
  ConfigureProxyBlanket(current_state.Get());
  LONG value = 0;
  *hresult = current_state->get_stateValue(&value);
  if (FAILED(*hresult))
    return false;
  *state_value = static_cast<CurrentState>(value);
  return true;
}

int main() {
  HRESULT hr = S_OK;
  COInitializeHolder com_holder;
  if (!com_holder) {
    return -1;
  }

  Microsoft::WRL::ComPtr<IClassFactory> class_factory;
  Microsoft::WRL::ComPtr<GoogleUpdate3Lib::IGoogleUpdate3Web> google_update;

  hr = ::CoGetClassObject(
      __uuidof(GoogleUpdate3Lib::GoogleUpdate3WebMachineClass), CLSCTX_ALL,
      nullptr, IID_PPV_ARGS(&class_factory));
  if (FAILED(hr)) {
    return -2;
  }

  hr = ConfigureProxyBlanket(class_factory.Get());
  if (FAILED(hr)) {
    return -3;
  }

  hr = class_factory->CreateInstance(nullptr, IID_PPV_ARGS(&(google_update)));
  if (FAILED(hr)) {
    return -4;
  }
  hr = ConfigureProxyBlanket(google_update.Get());
  if (FAILED(hr)) {
    return -5;
  }

  Microsoft::WRL::ComPtr<GoogleUpdate3Lib::IAppBundleWeb> app_bundle;
  Microsoft::WRL::ComPtr<IDispatch> dispatch;
  hr = google_update->raw_createAppBundleWeb(&dispatch);
  if (FAILED(hr)) {
    return -6;
  }
  hr = dispatch.As(&app_bundle);
  if (FAILED(hr)) {
    return -7;
  }
  dispatch.Reset();

  hr = app_bundle->raw_initialize();
  if (FAILED(hr)) {
    return -8;
  }

  // It is common for this call to fail with APP_USING_EXTERNAL_UPDATER if
  // an auto update is in progress.
  _bstr_t app_guid("{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}");  // WebView runtime
  hr = app_bundle->raw_createInstalledApp(app_guid);
  if (FAILED(hr)) {
    return -9;
  }

  hr = app_bundle->get_appWeb(0, &dispatch);
  if (FAILED(hr)) {
    return -10;
  }

  Microsoft::WRL::ComPtr<GoogleUpdate3Lib::IAppWeb> app;
  hr = dispatch.As(&app);
  if (FAILED(hr)) {
    return -11;
  }

  hr = ConfigureProxyBlanket(app.Get());
  if (FAILED(hr)) {
    return -12;
  }
  hr = app_bundle->raw_checkForUpdate();
  if (FAILED(hr)) {
    return -13;
  }

  for (int i = 1; i < 1000; ++i) {
    CurrentState state_value;
    if (!GetCurrentState(app.Get(), &state_value, &hr)) {
      return -13;
    }
       
    //_variant_t status;
    //hr = app->get_currentState(&status);
    //hr = VARIANT_BOOL im_a_busy_bundle;
    //HRESULT hr = app_bundle->raw_isBusy(&im_a_busy_bundle);
    if (state_value >= STATE_INSTALL_COMPLETE) {
      break;
    }

    ::Sleep(1000);
  }
}
