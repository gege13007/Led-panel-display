VERSION 5.00
Object = "{F9043C88-F6F2-101A-A3C9-08002B2F49FB}#1.2#0"; "COMDLG32.OCX"
Begin VB.Form Form1 
   BackColor       =   &H80000004&
   Caption         =   "Form1"
   ClientHeight    =   5160
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   11700
   ForeColor       =   &H8000000E&
   LinkTopic       =   "Form1"
   ScaleHeight     =   5160
   ScaleWidth      =   11700
   StartUpPosition =   3  'Windows Default
   Begin VB.CommandButton Command3 
      Caption         =   "Quitter"
      Height          =   495
      Left            =   2800
      TabIndex        =   19
      Top             =   4200
      Width           =   1215
   End
   Begin MSComDlg.CommonDialog CommonDialog1 
      Left            =   4320
      Top             =   4200
      _ExtentX        =   847
      _ExtentY        =   847
      _Version        =   393216
   End
   Begin VB.CommandButton Command2 
      Caption         =   "Fonte"
      Height          =   375
      Left            =   4560
      TabIndex        =   18
      Top             =   120
      Width           =   855
   End
   Begin VB.TextBox hmaxtxt 
      Alignment       =   2  'Center
      Height          =   315
      Left            =   600
      TabIndex        =   16
      Text            =   "64"
      Top             =   1920
      Width           =   360
   End
   Begin VB.TextBox codetxt 
      Alignment       =   2  'Center
      Height          =   315
      Left            =   600
      TabIndex        =   13
      Text            =   "W"
      Top             =   1150
      Width           =   375
   End
   Begin VB.TextBox tochartxt 
      Alignment       =   2  'Center
      Height          =   330
      Left            =   3360
      TabIndex        =   8
      Text            =   "z"
      Top             =   3360
      Width           =   495
   End
   Begin VB.TextBox fromchartxt 
      Alignment       =   2  'Center
      Height          =   330
      Left            =   1560
      TabIndex        =   7
      Text            =   " "
      Top             =   3360
      Width           =   495
   End
   Begin VB.PictureBox Picture1 
      AutoSize        =   -1  'True
      BorderStyle     =   0  'None
      Height          =   4950
      Left            =   5880
      Picture         =   "Form1.frx":0000
      ScaleHeight     =   3680.769
      ScaleMode       =   0  'User
      ScaleWidth      =   5715
      TabIndex        =   6
      Top             =   120
      Width           =   5715
   End
   Begin VB.TextBox tailletxt 
      Alignment       =   2  'Center
      Height          =   330
      Left            =   3800
      TabIndex        =   4
      Text            =   "62"
      Top             =   130
      Width           =   495
   End
   Begin VB.CommandButton Command1 
      Caption         =   "&Ecrire"
      Height          =   495
      Left            =   720
      TabIndex        =   3
      Top             =   4200
      Width           =   1500
   End
   Begin VB.PictureBox pic 
      Appearance      =   0  'Flat
      BackColor       =   &H80000012&
      BorderStyle     =   0  'None
      FillColor       =   &H00FFFFFF&
      FillStyle       =   0  'Solid
      ForeColor       =   &H80000000&
      Height          =   1500
      Left            =   1150
      ScaleHeight     =   100
      ScaleMode       =   3  'Pixel
      ScaleWidth      =   100
      TabIndex        =   1
      Top             =   960
      Width           =   1500
   End
   Begin VB.TextBox fonte 
      Height          =   330
      Left            =   1000
      TabIndex        =   0
      Text            =   "Times New Roman"
      Top             =   130
      Width           =   2200
   End
   Begin VB.Label Label6 
      Alignment       =   2  'Center
      Caption         =   "Char"
      Height          =   315
      Left            =   520
      TabIndex        =   17
      Top             =   840
      Width           =   460
   End
   Begin VB.Label Label5 
      Caption         =   "H max"
      Height          =   255
      Left            =   480
      TabIndex        =   15
      Top             =   1660
      Width           =   495
   End
   Begin VB.Label sizelbl 
      Caption         =   "Taille"
      Height          =   375
      Left            =   2760
      TabIndex        =   14
      Top             =   960
      Width           =   2895
   End
   Begin VB.Label tocharlbl 
      Alignment       =   2  'Center
      Height          =   255
      Left            =   3360
      TabIndex        =   12
      Top             =   3720
      Width           =   495
   End
   Begin VB.Label fromcharlbl 
      Alignment       =   2  'Center
      Height          =   255
      Left            =   1560
      TabIndex        =   11
      Top             =   3720
      Width           =   495
   End
   Begin VB.Label Label4 
      Caption         =   "jusqu'à"
      Height          =   375
      Left            =   2640
      TabIndex        =   10
      Top             =   3360
      Width           =   495
   End
   Begin VB.Label Label3 
      Caption         =   "De"
      Height          =   375
      Left            =   840
      TabIndex        =   9
      Top             =   3360
      Width           =   495
   End
   Begin VB.Label Label2 
      Caption         =   "Taille"
      Height          =   300
      Left            =   3300
      TabIndex        =   5
      Top             =   160
      Width           =   500
   End
   Begin VB.Label Label1 
      Caption         =   "Fonte"
      Height          =   255
      Left            =   300
      TabIndex        =   2
      Top             =   200
      Width           =   500
   End
End
Attribute VB_Name = "Form1"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

Function DirExist(DirName As String) As Boolean
    On Error GoTo ErrorHandler
    DirExist = GetAttr(DirName) And vbDirectory
ErrorHandler:
End Function

Public Sub drawlettre()
Dim c$, x%, y%, t%, r%, g%, b%, col

 If codetxt.Text = "" Then c$ = " " Else c$ = codetxt.Text
 code = Asc(c$)

 ' Draw on the picture.
 pic.Font.Name = fonte.Text
 pic.Font.Size = Val(tailletxt.Text)
 
 pic.Height = 2 + Val(hmaxtxt.Text) * Screen.TwipsPerPixelY ' pic.TextHeight(c$) * Screen.TwipsPerPixelY
 pic.Width = 2 + pic.TextWidth(c$) * Screen.TwipsPerPixelX
 
 sizelbl.Caption = "Size char: " + CStr(pic.ScaleWidth) + " x " + CStr(pic.ScaleHeight) + " pix"
 
 ' Center the text horizontally at the top
 ' of the picture.
 pic.Picture = LoadPicture()
 pic.Refresh
 pic.CurrentX = (pic.ScaleWidth - pic.TextWidth(c$)) / 2
 pic.CurrentY = -3 + (pic.ScaleHeight - pic.TextHeight(c$)) / 2
 pic.AutoRedraw = True
 pic.Print c$
 DoEvents
 
 'Passe en 2 couleurs
 For x = pic.ScaleWidth - 1 To 0 Step -1
   For y = 0 To pic.ScaleHeight - 1
      col = pic.Point(x, y)
      r = (col And &HFF0000) / 65536
      g = (col And 65280) / 256
      b = col And &HFF
      If (r + g + b) < limit Then pic.PSet (x, y), vbBlack Else pic.PSet (x, y), vbWhite
   Next y
 Next x
 
 ' Make the text a permanent part of the image.
 ' This is important if you later need to copy
 ' the picture to another control or the Printer.
 pic.Picture = pic.Image
End Sub

Private Sub codetxt_Change()
 drawlettre
End Sub

Private Sub Command1_Click()
 Dim fil$, x%, y%, n%, t$
 
 If Not DirExist(App.Path + "\fonte") Then MkDir App.Path + "\fonte"
 
 x = Asc(fromchartxt.Text)
 y = Asc(tochartxt.Text)
 For n = x To y
   codetxt.Text = Chr$(n)
   fil$ = "\fonte\0x" + UCase$(Hex$(code)) + ".gif"
   PicSave.SavePicture Me.pic, App.Path + fil$, fmtGIF
   DoEvents
 Next n
End Sub

Private Sub Command2_Click()
  CommonDialog1.Flags = cdlCFPrinterFonts 'Or cdlCFScreenFonts
  CommonDialog1.ShowFont
  fonte.Text = CommonDialog1.FontName
  tailletxt.Text = CommonDialog1.FontSize
End Sub

Private Sub Command3_Click()
 Unload Me
End Sub

Private Sub fonte_Change()
 drawlettre
End Sub

Private Sub Form_Load()
 drawlettre
 fromchartxt_Change
 tochartxt_Change
End Sub

Private Sub fromchartxt_Change()
 If fromchartxt.Text <> "" Then fromcharlbl.Caption = "0x" + Hex(Asc(fromchartxt.Text))
 Me.Refresh
End Sub

Private Sub tailletxt_Change()
 drawlettre
End Sub

Private Sub tochartxt_Change()
 If tochartxt.Text <> "" Then tocharlbl.Caption = "0x" + Hex(Asc(tochartxt.Text))
 Me.Refresh
End Sub
