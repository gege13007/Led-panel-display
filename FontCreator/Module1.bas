Attribute VB_Name = "Module1"
Option Explicit

Private Declare Function BitBlt Lib "gdi32" _
(ByVal hDestDC As Long, ByVal x As Long, ByVal y As Long, _
ByVal nWidth As Long, ByVal nHeight As Long, _
ByVal hSrcDC As Long, ByVal xSrc As Long, ByVal ySrc As Long, ByVal dwRop As Long) As Long

Global code%

'Limite r+g+b < 400 donne du Noir sinon Blanc
Global Const limit = 400

