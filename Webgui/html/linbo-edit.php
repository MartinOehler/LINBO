<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
  <head>
    <title>LINBO-Edit 1.0</title>
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

<?

include "linbo-common.inc";

# A start.conf editor
$os=array();
$config=array();
$oscount=0;

$tooltip = array(
 "Name" => "Kurzname des Betriebssystems",
 "Version" => "Version des Betrebssystems",
 "Description" => "Kurzbeschreibung des Betriebssystems",
 "Image" => "Dateiname des Betriebssystem-Image auf dem Server (Endung .cloop für Basisimage, .rsync für differentielles)",
 "Baseimage" => "Dateiname des Basis-Image auf dem Server (Endung .cloop)",
 "Boot" => "Partition, von der der Betriebssystem-Kern gebotet wird",
 "Root" => "Partition, auf der die Betriebssystem-Daten liegen (normalerweise identisch mit Boot)",
 "Kernel" => "Dateiname des Betriebssystem-Kerns (normalerweise grub.exe für Windows, vmlinuz für Linux)",
 "Initrd" => "Dateiname der initialen Ramdisk (Linux-spezifisch)",
 "Append" => "Kernel-Bootparameter, Betriebssystem-spezifisch",
 "Autostart" => "Soll dieses Betriebssystem automatisch gestartet werden?",
 "Startenabled" => "Soll das Betriebssystem gestartet werden können?",
 "Syncenabled" => "Soll das Betriebssystem gesynct werden können?",
 "Bootable" => "Soll das 'bootbar'-Partitions-Flag gesetzt werden? (Normalerweise nur für Windows notwendig)",
 "Dev" => "Partitionsname in Linux-Schreibweise (z.B. /dev/hda1 für 'C:'-Partition)",
 "Size" => "Partitionsgröße in kB, muss mindestens so groß sein wie das ausgepackte Betriebssystem-Image, das auf dieser Partition installiert werden soll. Leer = Rest der Platte",
 "Id" => "Partitions-Kennung (Hex-Zahl): 83=Linux, 82=Linux Swap, 7=NTFS, 5=Extended, 0=Leer",
 "Fstype" => "Dateisystem, das formatiert werden soll, z.B. reiserfs, ntfs, ext3, ext2, ... leer für kein Dateisystem",
 "Cache" => "Partitionsname der Cache-Partition in Linux-Schreibweise (z.B. /dev/hda6)",
 "Server" => "IP-Adresse des LINBO-Servers für RSYNC",
 "Roottimeout" => "Timeout in Sekunden, nach dem der Administrator automatisch ausgeloggt wird.",
 "Delete" => "Wenn aktiviert, wird der aktuelle Eintrag beim Speichern gelöscht",
 "save" => "Hier steht der Dateiname, unter dem die (geänderte) Datei gespeichert werden soll",
 "Serverport" => "IP-Adresse des Multicast-Servers:Basisport für dieses Image (ab 1024, und durch 2 teilbar)",
);

function read_section($f){
 $a=array();
 while(!feof($f)&&$l=fgets($f)) {
  $l = trim($l);
  if(!strlen($l)) break; # Empty line indicates end of section
  $l = strtok($l,"#"); # Strip comments
  $keyval = explode("=", $l, 2);
  if(count($keyval) > 0) {
   $key = trim($keyval[0]);
   $value = trim($keyval[1]);
   if(strlen($key) > 0) {
    $a[$key] = $value;
   }
  }
 }
 return $a;
}

# $filename is basename, $confdir will be prepended.
function read_conffile($filename) {
 global $os;
 global $partition;
 global $config;
 global $confdir;
 global $tooltip;
 $oscount = 0;
 $partitioncount = 0;
 $path = "$confdir/$filename";
 if(file_exists($path) && $f=fopen($path,"r")) {
  while(!feof($f)&&$line=fgets($f)) {
   $line = strtok($line,"#"); # Strip comments
   $line = trim($line);
   if(!strcasecmp($line,"[OS]")) {
    $tmp=array();
    $tmp = read_section($f);
    if(count($tmp)) {
     $os[$oscount++]=$tmp;
    }
   } elseif(!strcasecmp($line,"[Partition]")) {
    $tmp=array();
    $tmp = read_section($f);
    if(count($tmp)) {
     $partition[$partitioncount++]=$tmp;
    }
   } elseif(!strcasecmp($line,"[LINBO]")) {
    $tmp=array();
    $tmp = read_section($f);
    if(count($tmp)) {
     $config=$tmp;
    }
   }
  } 
  fclose($f);
 }
else
 {
  echo "<FONT COLOR=\"red\">".$filename." nicht gefunden.</FONT>";
 }
}

# print("<PRE>\n");
# print_r($os);
# print_r($partition);
# print_r($config);
# print("</PRE>\n");

function printformsection($var,$title,$array){
 global $tooltip;
 global $osadd;
 global $partitionadd;
 if(!strcmp($var,"partition") && !empty($partitionadd)) {
  $n = array(
   'Dev'      => '    # Linux device name',
   'Size'     => '    # Partition size in kB',
   'Id'       => '    # Partition type (83 = Linux, 82 = swap, c = FAT32, ...)',
   'Fstype'   => '    # File system on Partition (reiserfs, ntfs, ...)',
   'Bootable' => 'no  # no = Mark this partition as non-bootable (or Linux)'
   );
  array_push($array, $n);
 }
 if(!strcmp($var,"os") && !empty($osadd)) {
  $n = array(
   'Name'           => '  # Name of OS',
   'Description'    => '  # Descriptive Text',
   'Version'        => '  # Version/Date of OS (optional)',
   'Image'          => '  # Filename of rsync batch',
   'BaseImage'      => '  # Filename of base partition image',
   'Boot'           => '  # Partition containing Kernel & Initrd',
   'Root'           => '  # root=/dev/partition Parameter (Root FS)',
   'Kernel'         => '  # Relative filename of Kernel or Boot image',
   'Initrd'         => '  # Relative filename of Initrd',
   'Append'         => '  # Kernel-specific',
   'StartEnabled'   => '  # Enable "Start" Button',
   'SyncEnabled'    => '  # Enable "Synchronize" Button',
   'RemoteSyncEnabled' => ' # Enable "Synchronize from Server" Button'
   );
  array_push($array, $n);
 }
 $count=0;
 print(" <table>\n");
 foreach($array as $a) {
  if(!empty($a['Delete']) && !strcmp($a['Delete'],"on")) continue; # Skip deleted
  print("  <tr><th colspan=\"2\">".htmlspecialchars($title)."</th></tr>\n");
  $index=htmlspecialchars($var)."[".($count++)."]";
  foreach ($a as $k => $v) {
   $k = ucfirst(strtolower($k)); # key
   $l = htmlspecialchars($k);    # label (HTML-ized)
   $key=$index."[$l]";
   $v = htmlspecialchars($v); # value (HTML-ized)
   print("  <tr><th width=300 align=\"left\">$l</th> <td>");
   print("<abbr title=\"".($tooltip[$k]?$tooltip[$k]:"")."\">");
   if(strstr($k,"enabled")||strstr($k,"able")||strstr($k,"start")) {
    print("<input type=\"checkbox\" name=\"$key\" id=\"$key\"");
    print(!strcasecmp($v, "yes")?" checked":"");
    print(" />");
   } else {
    print("<input type=\"text\" name=\"$key\" id=\"$key\" value=\"$v\" size=\"80\" />");
   }
   print("</abbr></td></tr>\n");
  }
  if(strcmp($var,"config")) { # Do NOT delete [LINBO]
   $delkey=$index."[Delete]";
   print("  <tr><th align=\"left\"><font color=\"red\">Zum Löschen markieren</font></th> <td><abbr title=\"".($tooltip['Delete'])."\"><input type=\"checkbox\" name=\"$delkey\" id=\"$delkey\"></abbr></td></tr>\n");
  }
  print("  <tr><td>&nbsp;</td><td>&nbsp;</td></tr>\n");
 }
 print(" </table>\n");
}

function save() { # Save a .conf file
 global $newfile;
 global $os;
 global $partition;
 global $config;
 global $confdir;
 $path = "$confdir/$newfile";

 if($f = fopen("$path","w")) {
  fwrite($f, "[LINBO]\n");
  foreach($config as $k => $v) fwrite($f, "$k = $v\n");
  fwrite($f, "\n");
  foreach($partition as $p) {
   if(!empty($p['Delete']) && !strcmp($p['Delete'],"on")) continue; # Skip deleted
   fwrite($f, "[Partition]\n");
   foreach($p as $k => $v) fwrite($f, "$k = $v\n");
   fwrite($f, "\n");
  }
  fwrite($f, "\n");
  foreach($os as $o) {
   if(!empty($o['Delete']) && !strcmp($o['Delete'],"on")) continue; # Skip deleted
   fwrite($f, "[OS]\n");
   foreach($o as $k => $v) fwrite($f, "$k = $v\n");
   fwrite($f, "\n");
  }
  fwrite($f, "\n");
  fclose($f);
  print("<h1 align=center><font color=\"green\">$path erfolgreich gespeichert.</h1>");
 } else {
  print("<h1 align=center><font color=\"red\">Fehler: Kann nicht in Datei $path schreiben!</h1>");
 }
}

function editform(){
 global $file;
 global $newfile;
 global $os;
 global $partition;
 global $partitionadd;
 global $sadd;
 global $config;
 global $tooltip;
 print("<form method=\"post\">");
 print(" <input type=\"hidden\" name=\"file\" id=\"file\" value=\"".htmlspecialchars($file)."\">\n");
 print("<h2>".htmlspecialchars($file)."</h2>\n");
 print("<h3>Allgemeine Einstellungen</h3>\n");
 $configarray=array($config);
 printformsection("config","[LINBO]", $configarray);
 print("<h3>Partitionen</h3>\n");
 printformsection("partition","[Partition]", $partition);
 print(" <input type=\"submit\" name=\"partitionadd\" id=\"partitionadd\" value=\"Partition hinzufügen\">\n");
 print("<h3>Betriebssysteme</h3>\n");
 printformsection("os","[OS]", $os);
 print(" <input type=\"submit\" name=\"osadd\" id=\"osadd\" value=\"OS hinzufügen\">\n");
 print(" <br /><br />\n");
 print(" <input type=\"submit\" name=\"save\" id=\"save\" value=\"Geänderte ".htmlspecialchars($file)." speichern als:\">");
 print(" <abbr title=\"".$tooltip['save']."\"><input type=\"text\" name=\"newfile\" id=\"newfile\" size=\"80\" value=\"".htmlspecialchars($newfile)."\"></abbr>\n");
 print("</form>\n");
}

function read_multicast() {
 global $confdir;
 global $mclist;
 $path = "$confdir/multicast.list";
 if(file_exists($path) && $f=fopen($path,"r")) {
  while(!feof($f)&&$line=fgets($f)) {
   $line = strtok($line,"\#"); # Strip comments
   $line = trim($line);
   $entries = preg_split("/[\s]+/", $line, -1, PREG_SPLIT_NO_EMPTY);
   # $entries = preg_split("/[\s]+/", $line);
   if(!empty($entries) && strlen($entries[0]) && strlen($entries[1])) {
    array_push($mclist, $entries);
   }
  }
  fclose($f);
 }
else
 {
  print(" <FONT COLOR=\"red\">multicast.list nicht gefunden.</FONT><br />");
 }
}

function filemenu($name, $pattern, $selectedfile) {
 global $confdir;
 if ($handle = opendir($confdir)) {
  print(" <select name=\"$name\" id=\"$name\">\n");
  while (false !== ($file = readdir($handle))) {
   if(preg_match($pattern,"$file")) {
    $pfile=htmlspecialchars($file); # Safe
    print("  <option value=\"$pfile\"");
    if(!strcmp($selectedfile, $file)) print(" selected");
    print(">$pfile</option>\n");
   }
  }
 }
 closedir($handle);
 print(" </select>\n");
}

function multicastform() {
 global $config;
 global $confdir;
 global $tooltip;
 global $mclist;
 global $mcadd;
 $count=0;
 print("<h1>multicast.list Einstellungen</h1>\n");
 print("<form method=\"post\">\n");
 print(" <table>\n");
 foreach($mclist as $m) {
  if(!empty($m[0])) { # Skip empty fields
   print("  <tr><th width=300 align=\"left\">");
   filemenu("mclist[$count][0]","/\.(cloop|rsync)$/",$m[0]);
   print("</th> <td>");
   print("<abbr title=\"".$tooltip['Serverport']."\">");
   print("<input type=\"text\" name=\"mclist[$count][1]\" id=\"mclist[$count][1]\" value=\"".$m[1]."\" size=\"80\" />");
   print("</abbr></td></tr>\n");
   print("  <tr><td>&nbsp;</td><td>&nbsp;</td></tr>\n");
   $count++;
  }
 }
 if(!empty($mcadd)) {
  print("  <tr><th width=300 align=\"left\">");
  filemenu("mclist[$count][0]","/\.(cloop|rsync)$/","none");
  print("</th> <td>");
  print("<abbr title=\"".$tooltip['Serverport']."\">");
  print("<input type=\"text\" name=\"mclist[$count][1]\" id=\"mclist[$count][1]\" size=\"80\" />");
  print("</abbr></td></tr>\n");
 }
 print(" </table>\n");
 print(" <input type=\"submit\" name=\"mcadd\" id=\"mcadd\" value=\"Eintrag hinzufügen\">\n");
 print(" <br /><br />\n");
 print(" <input type=\"submit\" name=\"savemc\" id=\"savemc\" value=\"Geänderte multicast.list speichern\">");
 print("</form>\n");
}

function savemc() { # Save multicast.list
 global $mclist;
 global $confdir;
 $path = "$confdir/multicast.list";

 if($f = fopen("$path","w")) {
  foreach($mclist as $a) {
   if(strlen($a[0]) &&strlen($a[1])) fwrite($f, $a[0]." ".$a[1]."\n");
  }
  fclose($f);
  print("<h1 align=center><font color=\"green\">$path erfolgreich gespeichert.</h1>");
 } else {
  print("<h1 align=center><font color=\"red\">Fehler: Kann nicht in Datei $path schreiben!</h1>");
 }
}

# Main
$file = $_POST['file'];
if(!strlen($file)) $file = $_GET['file'];
$edit = $_POST['edit'];
if(!strlen($edit)) $edit = $_GET['edit'];
$editgroups = $_POST['editgroups'];
if(!strlen($editgroups)) $editgroups = $_GET['editgroups'];
$editmc = $_POST['editmc'];
if(!strlen($editmc)) $editmc = $_GET['editmc'];
$partitionadd = $_POST['partitionadd'];
$osadd = $_POST['osadd'];
$save = $_POST['save'];
$savemc = $_POST['savemc'];
$mcadd = $_POST['mcadd'];
$newfile = $_POST['newfile'];
if(!strlen($newfile)) $newfile = $file;

# DEBUG
# print("<pre>POST:\n"); print_r($_POST);
# print("\n\nGET:\n"); print_r($_GET);
# print("</pre>\n");

# Edit configuration file dialog
if(!empty($edit)) { 
 read_conffile($file);
 editform();
} elseif(!empty($osadd)||!empty($partitionadd)) {
 $os = $_POST['os'];
 $partition = $_POST['partition'];
 $config = $_POST['config'][0];
 editform();
} elseif(!empty($save)) {
 $os = $_POST['os'];
 $partition = $_POST['partition'];
 $config = $_POST['config'][0];
 save();
} elseif(!empty($editgroups)) {
} elseif(!empty($editmc)) {
 $mclist=array();
 read_multicast();
 multicastform();
} elseif(!empty($mcadd)) {
 $mclist = $_POST['mclist'];
 multicastform();
} elseif(!empty($savemc)) {
 $mclist = $_POST['mclist'];
 savemc();
}

print("<br />\n");

print("<form method=\"post\" action=\"linbo-admin.php\">");
print(" <input type=\"hidden\" name=\"file\" id=\"file\" value=\"".htmlspecialchars($file)."\">\n");
print(" <input type=\"submit\" value=\"Zurück zur LINBO-Hauptseite\">\n");
print("</form>\n");
?>

</body>
</html>
