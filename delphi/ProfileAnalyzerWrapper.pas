unit ProfileAnalyzerWrapper;

interface

type
  TProfileInfo = record
    ProfileType: string;
    NominalHeight: Double;
    WeightPerMeter: Double;
  end;

function AnalyzeProfile(const AProfileName: string;
  out AInfo: TProfileInfo): Boolean; overload;
function AnalyzeProfile(const AProfileName: string;
  out AInfo: TProfileInfo; out AError: string): Boolean; overload;

implementation

uses
  Winapi.Windows, System.SysUtils, System.Math;

const
  // These values must match profile_analyzer_api.h; capacity includes the null.
  ProfileTypeCapacity = 32;
  StatusSuccess = 0;
  StatusInvalidArgument = 1;
  StatusInvalidFormat = 2;
  StatusUnsupportedProfile = 3;
  StatusInternalError = 4;

type
  TAnalyzeProfile = function(ProfileName: PWideChar;
    NominalHeight, WeightPerMeter: PDouble; ProfileType: PWideChar): Integer; cdecl;

function StatusMessage(Status: Integer): string;
begin
  case Status of
    StatusInvalidArgument:
      Result := 'The profile analyzer rejected its arguments.';
    StatusInvalidFormat:
      Result := 'Invalid profile name or dimensions. Examples: IPE200, HEA160, L150*5.';
    StatusUnsupportedProfile:
      Result := 'This profile type is not supported by the analyzer.';
    StatusInternalError:
      Result := 'The profile analyzer reported an internal error.';
  else
    Result := Format('The profile analyzer returned an unknown status (%d).', [Status]);
  end;
end;

function AnalyzeProfile(const AProfileName: string;
  out AInfo: TProfileInfo; out AError: string): Boolean;
var
  LibraryPath: string;
  LibraryHandle: HMODULE;
  LoadError: DWORD;
  Analyze: TAnalyzeProfile;
  ProfileType: array[0..ProfileTypeCapacity - 1] of WideChar;
  Height, Weight: Double;
  Status, TypeLength: Integer;
begin
  Result := False;
  AInfo.ProfileType := '';
  AInfo.NominalHeight := 0;
  AInfo.WeightPerMeter := 0;
  AError := '';

  // A Delphi string can contain nulls; the C ABI would truncate it silently.
  if Pos(#0, AProfileName) <> 0 then
  begin
    AError := 'The profile name must not contain a null character.';
    Exit;
  end;

  LibraryPath := ExtractFilePath(ExpandFileName(ParamStr(0))) + 'ProfileAnalyzer.dll';
  LibraryHandle := LoadLibraryW(PWideChar(LibraryPath));
  if LibraryHandle = 0 then
  begin
    LoadError := GetLastError;
    AError := Format('Could not load "%s" (Windows error %d).', [LibraryPath, LoadError]);
    Exit;
  end;

  try
    @Analyze := GetProcAddress(LibraryHandle, 'AnalyzeProfile');
    if not Assigned(Analyze) then
    begin
      AError := 'The profile analyzer does not export AnalyzeProfile.';
      Exit;
    end;

    FillChar(ProfileType, SizeOf(ProfileType), 0);
    Height := 0;
    Weight := 0;
    Status := Analyze(PWideChar(AProfileName), @Height, @Weight, @ProfileType[0]);
    if Status <> StatusSuccess then
    begin
      AError := StatusMessage(Status);
      Exit;
    end;

    TypeLength := 0;
    while TypeLength < ProfileTypeCapacity do
    begin
      if ProfileType[TypeLength] = #0 then
        Break;
      Inc(TypeLength);
    end;
    if (TypeLength = 0) or (TypeLength = ProfileTypeCapacity)
      or IsNan(Height) or IsInfinite(Height) or (Height <= 0)
      or IsNan(Weight) or IsInfinite(Weight) or (Weight <= 0) then
    begin
      AError := 'The profile analyzer returned an invalid result.';
      Exit;
    end;

    SetString(AInfo.ProfileType, PWideChar(@ProfileType[0]), TypeLength);
    AInfo.NominalHeight := Height;
    AInfo.WeightPerMeter := Weight;
    Result := True;
  finally
    FreeLibrary(LibraryHandle);
  end;
end;

function AnalyzeProfile(const AProfileName: string;
  out AInfo: TProfileInfo): Boolean;
var
  Error: string;
begin
  Result := AnalyzeProfile(AProfileName, AInfo, Error);
end;

end.
