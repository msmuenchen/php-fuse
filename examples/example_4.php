<?php
//See https://github.com/jrk/passfs/blob/master/opts.c for the C equivalent of this file
if (!extension_loaded("fuse")) dl("fuse.so");
error_reporting(E_ALL);
class Sample4Fuse extends Fuse {
 
 public function __construct() {
  //The enum at line 27, as PHP doesn't know enums, use $i as hack
  //Do not forget $i++, else opt_parse will whine.
  $i=0;
  $this->argopt_keys=array("KEY_STATS"=>$i++,
                     "KEY_HELP"=>$i++,
                     "KEY_FUSE_HELP"=>$i++,
                     "KEY_VERSION"=>$i++,
                     "KEY_DEBUG"=>$i++,
                     "KEY_MONITOR"=>$i++,
                     "KEY_MONITOR_FILE"=>$i++,
                     "KEY_DIR_METHOD2"=>$i++,
                     "KEY_DEMO_INT"=>$i++,
                     "KEY_DEMO_STRING"=>$i++,
                     "KEY_DEMO_SPACE"=>$i++
                     );
  $this->argopts=array("--help"=>$this->argopt_keys["KEY_HELP"],
                       "--version"=>$this->argopt_keys["KEY_VERSION"],
                       "-h"=>$this->argopt_keys["KEY_HELP"],
                       "-H"=>$this->argopt_keys["KEY_FUSE_HELP"],
                       "-V"=>$this->argopt_keys["KEY_VERSION"],
                       "stats"=>$this->argopt_keys["KEY_STATS"],
                       "-d"=>$this->argopt_keys["KEY_DEBUG"],
                       "-m"=>$this->argopt_keys["KEY_MONITOR"],
                       "-m="=>$this->argopt_keys["KEY_MONITOR_FILE"],
                       "-D"=>$this->argopt_keys["KEY_DIR_METHOD2"],
                       "-n "=>$this->argopt_keys["KEY_DEMO_SPACE"]
  );
  $this->dataopts=array(array("templ"=>"-s=%s","value"=>"","key"=>$this->argopt_keys["KEY_DEMO_STRING"]),
                        array("templ"=>"-i=%lu","value"=>0,"key"=>$this->argopt_keys["KEY_DEMO_INT"])
  );
 }
 public function main($argc,$argv) {   
  printf("argc is %d, argv is ('%s')\n",$argc,implode("', '",$argv));
  $res=$this->opt_parse($argc,$argv,$this->dataopts,$this->argopts,array($this,"opt_proc"));
  if($res===false) {
    printf("Error in opt_parse\n");
    exit;
  }
  printf("argc now %d, argv now ('%s')\n",$argc,implode("', '",$argv));
 }
 public function opt_proc($data,$arg,$key,&$argc,&$argv) {
   printf("opt_proc called. arg is '%s', key is %d, argc is %d, argv is ('%s'), data is\n%s\n",$arg,$key,$argc,implode("', '",$argv),print_r($data,true));
   $argv[0].="fail";
   return 1;
 }
  public function getdir($path, &$retval) { 
    if ($path != "/") {
      return -FUSE_ENOENT;
    }
    $retval["."] = array('type' => FUSE_DT_DIR);
    $retval[".."] = array('type' => FUSE_DT_DIR);
    $retval["test.txt"] = array('type' => FUSE_DT_REG);
    return 0;
  } 

  public function getattr($path, &$st) { 
    $st['dev'] = 0;
    $st['ino'] = 0;
    $st['mode'] = 0;
    $st['nlink'] = 0;
    $st['uid'] = 0;
    $st['gid'] = 0;
    $st['rdev'] = 0;
    $st['size'] = 0;
    $st['atime'] = 0;
    $st['mtime'] = 0;
    $st['ctime'] = 0;
    $st['blksize'] = 0;
    $st['blocks'] = 0;

    if ($path == "/") {
      $st['mode'] = FUSE_S_IFDIR | 0775; 
      $st['nlink'] = 3;
      $st['size'] = 0;
    } else if ($path == "/test.txt") {
      $st['mode'] = FUSE_S_IFREG | 0664; 
      $st['nlink'] = 1;
      $st['size'] = 12; 
    }

    return 0;
  }
  public function open($path, $mode) {
    return 1;
  }

  public function read($path, $fh, $offset, $buf_len, &$buf) {
    $buf = "hello world\n";
    return strlen($buf);
  }

  public function release($path, $fh) {
    return 0;
  }
}


$fuse = new Sample4Fuse();
$fuse->main($argc,$argv);
?>
