VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
  Persistable = 0  'NotPersistable
  DataBindingBehavior = 0  'vbNone
  DataSourceBehavior  = 0  'vbNone
  MTSTransactionMode  = 0  'NotAnMTSObject
END
Attribute VB_Name = "PicSave"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit
'
'=======
'PicSave
'=======
'
'A global predeclared object providing a simple SavePicture() method
'based on GDI+ for saving bitmap StdPicture objects in GIF, PNG, and
'JPEG formats.
'

Private Const API_FALSE As Long = 0
Private Const API_TRUE As Long = 1
Private Const API_NULL As Long = 0

Private Const S_OK As Long = 0

Private Type IID
    Data1 As Long
    Data2 As Integer
    Data3 As Integer
    Data4(0 To 7) As Byte
End Type

Private Declare Function CLSIDFromString Lib "ole32" ( _
    ByVal lpsz As Long, _
    ByRef clsid As IID) As Long

Private Enum STGM
    STGM_READ = &H0&
    STGM_WRITE = &H1&
    STGM_READWRITE = &H2&
    
    STGM_SHARE_EXCLUSIVE = &H10&
    STGM_SHARE_DENY_WRITE = &H20&
    STGM_SHARE_DENY_READ = &H30&
    STGM_SHARE_DENY_NONE = &H40&
    
    STGM_FAILIFTHERE = &H0&
    STGM_CREATE = &H1000&
End Enum

Private Declare Function SHCreateStreamOnFile Lib "shlwapi" Alias "SHCreateStreamOnFileW" ( _
    ByVal pszFile As Long, _
    ByVal grfMode As STGM, _
    ByRef stm As IUnknown) As Long

Private Enum GdiplusStatus
    Ok = 0
    GenericError = 1
    InvalidParameter = 2
    OutOfMemory = 3
    ObjectBusy = 4
    InsufficientBuffer = 5
    NotImplemented = 6
    Win32Error = 7
    WrongState = 8
    Aborted = 9
    FileNotFound = 10
    ValueOverflow = 11
    AccessDenied = 12
    UnknownImageFormat = 13
    FontFamilyNotFound = 14
    FontStyleNotFound = 15
    NotTrueTypeFont = 16
    UnsupportedGdiplusVersion = 17
    GdiplusNotInitialized = 18
    PropertyNotFound = 19
    PropertyNotSupported = 20
End Enum

Private Type GdiplusStartupInput
    GdiplusVersion As Long
    DebugEventCallback As Long
    'Boolean value that specifies whether to suppress the GDI+ background
    'thread. If you set this member to TRUE, GdiplusStartup returns (in
    'its output parameter) a pointer to a hook function and a pointer to
    'an unhook function. You must call those functions appropriately to
    'replace the background thread. If you do not want to be responsible
    'for calling the hook and unhook functions, set this member to FALSE:
    SuppressBackgroundThread As Long
    SuppressExternalCodecs As Long
End Type

Private Enum EncoderParameterValueType
    EncoderParameterValueTypeByte = 1
    EncoderParameterValueTypeASCII = 2
    EncoderParameterValueTypeShort = 3
    EncoderParameterValueTypeLong = 4
    EncoderParameterValueTypeRational = 5
    EncoderParameterValueTypeLongRange = 6
    EncoderParameterValueTypeUndefined = 7
    EncoderParameterValueTypeRationalRange = 8
End Enum

Private Type EncoderParameter
    EncoderGUID As IID
    NumberOfValues As Long
    Type As EncoderParameterValueType
    pValue As Long
End Type

Private Type EncoderParameters
    Count As Long 'Must always be set to 0 or 1 here, we have just one declared below.
    Parameter As EncoderParameter
End Type

Private Declare Function GdipCreateBitmapFromHBITMAP Lib "gdiplus" ( _
    ByVal hbm As Long, ByVal hpal As Long, ByRef pBitMap As Long) As Long

Private Declare Function GdipDisposeImage Lib "gdiplus" ( _
    ByVal gdipImage As Long) As GdiplusStatus

Private Declare Function GdipSaveImageToStream Lib "gdiplus" ( _
    ByVal gdipImage As Long, _
    ByVal Stream As IUnknown, _
    ByRef clsidEncoder As IID, _
    ByVal pEncoderParams As Long) As GdiplusStatus

Private Declare Function GdiplusStartup Lib "gdiplus" ( _
    ByRef gdipToken As Long, _
    ByRef StartupInput As GdiplusStartupInput, _
    ByVal pStartupOutput As Long) As GdiplusStatus

Private Declare Function GdiplusShutdown Lib "gdiplus" ( _
    ByVal gdipToken As Long) As GdiplusStatus

Private ImageFormatGIF As IID
Private ImageFormatJPEG As IID
Private ImageFormatPNG As IID
Private EncoderQuality As IID
Private gdipStartupInput As GdiplusStartupInput
Private gdipToken As Long
Private IID_IPicture As IID

Public Enum SavePictureFormats
    [_fmtMin] = 0
    fmtGIF = 0
    fmtJPEG
    fmtPNG
    [_fmtMax] = fmtPNG
End Enum
#If False Then
Dim fmtGIF, fmtJPEG, fmtPNG
#End If

Public Sub SavePicture( _
    ByVal StdPicture As StdPicture, _
    ByVal FileName As String, _
    ByVal Format As SavePictureFormats, _
    Optional ByVal JpegQualityPct As Long = 70)

    Dim HRESULT As Long
    Dim gdipBitmap As Long
    Dim Stream As IUnknown
    Dim Params As EncoderParameters
    Dim pParams As Long
    Dim EncoderGUID As IID

    If gdipToken = 0 Then Err.Raise &H80044800, TypeName(Me), "GDI+ failed to start"
    If (StdPicture Is Nothing) Or _
       ([_fmtMin] > Format Or Format > [_fmtMax]) Or _
       (1 > JpegQualityPct Or JpegQualityPct > 100) Then Err.Raise 5

    HRESULT = GdipCreateBitmapFromHBITMAP(StdPicture.Handle, StdPicture.hpal, gdipBitmap)
    If HRESULT <> Ok Then
        Err.Raise &H80044848, TypeName(Me), _
                  "GdipCreateBitmapFromHBITMAP error " & CStr(HRESULT)
    End If
    HRESULT = SHCreateStreamOnFile(StrPtr(FileName), _
                  STGM_CREATE Or STGM_WRITE Or STGM_SHARE_EXCLUSIVE, Stream)
    If HRESULT <> S_OK Then
        GdipDisposeImage gdipBitmap
        Err.Raise &H80044850, TypeName(Me), _
                  "SHCreateStreamOnFile error " & CStr(HRESULT)
    End If
    Set Stream = Nothing 'Force creation on 64-bit Windows.
    HRESULT = SHCreateStreamOnFile(StrPtr(FileName), _
                  STGM_CREATE Or STGM_WRITE Or STGM_SHARE_EXCLUSIVE, Stream)
    If HRESULT <> S_OK Then
        GdipDisposeImage gdipBitmap
        Err.Raise &H80044854, _
                  TypeName(Me), _
                  "SHCreateStreamOnFile error " & CStr(HRESULT)
    End If
    
    With Params
        If Format = fmtJPEG Then
            EncoderGUID = ImageFormatJPEG
            .Count = 1
            With .Parameter
                .EncoderGUID = EncoderQuality
                .NumberOfValues = 1
                .Type = EncoderParameterValueTypeLong
                .pValue = VarPtr(JpegQualityPct)
            End With
            pParams = VarPtr(Params)
        ElseIf Format = fmtGIF Then
            EncoderGUID = ImageFormatGIF
        ElseIf Format = fmtPNG Then
            EncoderGUID = ImageFormatPNG
        End If
    End With
    HRESULT = GdipSaveImageToStream(gdipBitmap, Stream, EncoderGUID, pParams)
    If HRESULT <> Ok Then
        GdipDisposeImage gdipBitmap
        Err.Raise &H80044858, _
                  TypeName(Me), _
                  "GdipSaveImageToStream error " & CStr(HRESULT)
    End If
    GdipDisposeImage gdipBitmap
End Sub

Private Sub Class_Initialize()
    CLSIDFromString StrPtr("{7BF80980-BF32-101A-8BBB-00AA00300CAB}"), IID_IPicture
    CLSIDFromString StrPtr("{557CF402-1A04-11D3-9A73-0000F81EF32E}"), ImageFormatGIF
    CLSIDFromString StrPtr("{557CF401-1A04-11D3-9A73-0000F81EF32E}"), ImageFormatJPEG
    CLSIDFromString StrPtr("{557CF406-1A04-11D3-9A73-0000F81EF32E}"), ImageFormatPNG
    CLSIDFromString StrPtr("{1D5BE4B5-FA4A-452D-9CDD-5DB35105E7EB}"), EncoderQuality
    gdipStartupInput.GdiplusVersion = 1
    If GdiplusStartup(gdipToken, gdipStartupInput, API_NULL) <> Ok Then gdipToken = 0
End Sub

Private Sub Class_Terminate()
    If gdipToken <> 0 Then GdiplusShutdown gdipToken
End Sub

