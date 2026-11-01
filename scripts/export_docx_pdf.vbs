' Export the 16-page project report DOCX to PDF using the installed copy of Microsoft Word.
' Usage: cscript //nologo scripts\export_docx_pdf.vbs

Dim fso, root, docxPath, pdfPath, word, doc
Set fso = CreateObject("Scripting.FileSystemObject")
root = fso.GetParentFolderName(fso.GetParentFolderName(WScript.ScriptFullName))
docxPath = root & "\docs\Shridhar_Kumar_Project_Report_16pages.docx"
pdfPath = root & "\docs\Shridhar_Kumar_Project_Report_16pages.pdf"

If Not fso.FileExists(docxPath) Then
  WScript.Echo "DOCX not found: " & docxPath
  WScript.Quit 1
End If

Set word = CreateObject("Word.Application")
word.Visible = False
word.DisplayAlerts = 0

On Error Resume Next
Set doc = word.Documents.Open(docxPath, False, True)
If Err.Number <> 0 Then
  WScript.Echo "Open failed: " & Err.Description
  word.Quit
  WScript.Quit 1
End If

doc.Fields.Update
doc.Repaginate

Dim pages
pages = doc.ComputeStatistics(2) ' wdStatisticPages

doc.ExportAsFixedFormat pdfPath, 17 ' wdExportFormatPDF

If Err.Number <> 0 Then
  WScript.Echo "Export failed: " & Err.Description
  doc.Close 0
  word.Quit
  WScript.Quit 1
End If

doc.Close 0
word.Quit
On Error Goto 0

WScript.Echo "PAGES=" & pages
WScript.Echo "PDF=" & pdfPath
