program ProfileAnalyzerDemo;

{$APPTYPE CONSOLE}

uses
  System.SysUtils,
  ProfileAnalyzerWrapper in 'ProfileAnalyzerWrapper.pas';

procedure ShowProfile(const Name: string);
var
  Info: TProfileInfo;
  Error: string;
  Settings: TFormatSettings;
begin
  if not AnalyzeProfile(Name, Info, Error) then
  begin
    Writeln(ErrOutput, Name, ': ', Error);
    ExitCode := 1;
    Exit;
  end;

  Settings := TFormatSettings.Create;
  Settings.DecimalSeparator := '.';
  Writeln(Name);
  Writeln('  Type: ', Info.ProfileType);
  Writeln('  Nominal height: ', FloatToStr(Info.NominalHeight, Settings), ' mm');
  Writeln('  Estimated weight: ', FloatToStr(Info.WeightPerMeter, Settings), ' kg/m');
end;

var
  Index: Integer;
begin
  try
    Writeln('Profile analyzer demo. Weights are demonstration estimates, not catalog values.');
    if ParamCount = 0 then
    begin
      ShowProfile('IPE200');
      ShowProfile('HEA160');
      ShowProfile('L150*5');
    end
    else
      for Index := 1 to ParamCount do
        ShowProfile(ParamStr(Index));
  except
    on E: Exception do
    begin
      Writeln(ErrOutput, E.ClassName, ': ', E.Message);
      ExitCode := 1;
    end;
  end;
end.
