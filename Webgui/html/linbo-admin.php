<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
  <head>
    <title>LINBO-Admin 1.0</title>
    <link rel="stylesheet" href="paedML_style.css" type="text/css">
    <meta name="copyright" content="Copyright ©2007 Landesmedienzentrum BW">
    <meta name="description" content="paedML LINBO-Admin 1.0">
  </head>
  <body>
<!--    <table class="titlepage" cellspacing="0" cellpadding="0">
      <div style="background-image: url(paedML_titlepage.png); width: 600px; height: 600px; border: 1px solid gray; margin: 0px;">
	<div style="margin-left: 270px; margin-top:350px; font-family: Arial, sans-serif; text-decoration: none; font-weight: bold; font-size: 1.0em; line-height: 1.5em; font-color: gray;">Willkommen auf @@servername@@.@@domainname@@<br/>
	</div>
      </div>
  </table>
-->

<H1 ALIGN=CENTER>LINBO Admin V1.0</H1>

Hier können Sie die Client-Konfigurationen und die Multicast-Dateiliste für LINBO verwalten.
<br /><br />
Bitte wählen Sie eine Konfigurationsdatei aus dem Menü in untenstehener Tabelle aus, und wählen Sie "Bearbeiten" oder "Rechnergruppen zusammenfassen".
<br /><br />

<div align=center>

<?
include "linbo-common.inc";
$selectedfile = $_POST['file'];
if(!strlen($selectedfile)) $selectedfile = $_GET['file'];

if ($handle = opendir($confdir)) {
 print("<form action=\"linbo-edit.php\" method=\"post\">\n");
 print("<table border=\"1\" align=center width=\"600\">\n");
 print("<tr><td>");
 print(" <select name=\"file\" id=\"file\">\n");
 /* This is the correct way to loop over the directory. */
 while (false !== ($file = readdir($handle))) {
  if(preg_match('/.*\.conf.*/',"$file")) {
   if(is_file("$confdir/$file") && !is_link("$confdir/$file")) {
    $pfile=htmlspecialchars($file); # Safe
    print("  <option value=\"$pfile\"");
    if(!strcmp($selectedfile, $file)) print(" selected");
    print(">$pfile</option>\n");
   }
  }
 }
 closedir($handle);
 print("  <option value=\"template-start.conf\">Neu von Vorlage</option>\n");
 print(" </select>\n");
 print("</td><td nowrap>");
 print(" <abbr title=\"".($tooltip['edit'])."\"><input type=\"submit\" value=\"Bearbeiten\" name=\"edit\" id=\"edit\"></abbr>\n");
 print(" <abbr title=\"".($tooltip['editgroups'])."\"><input type=\"submit\" value=\"Rechner-Gruppen mit dieser Konfiguration verwalten\" name=\"editgroups\" id=\"editgroups\"></abbr>\n");
 print("</td></tr>\n");
  print("<tr><td colspan=\"2\" align=\"center\"><abbr title=\"".($tooltip['editmc'])."\"><input type=\"submit\" value=\"Multicast-Server Dateiliste bearbeiten\" name=\"editmc\" id=\"editmc\"></abbr></td></tr>\n");
 print("</table>\n");
 print("</form>\n");
}

$tooltip = array(
 "edit" => "Rechnerspezifische start.conf-Datei bearbeiten (öffnet weiteren Dialog)",
 "editgroups" => "Zusammefassen von Rechnern, die die gleiche Konfiguration benutzen, nach IP-Adresse (öffnet weiteren Dialog)",
 "editmc" => "multicast.list bearbeiten, um Images per udpcast im Netz effizient zu verteilen."
);

?>

 <br />

</div>
  
</body>
</html>
