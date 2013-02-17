<?php
class My extends Fuse {
	/* flags are passed like -flag on the command line */
	private $flags;
	/* options are passed like --option=value on the command line */
	private $options;
	/* params are passed like arguments on the command line */
	private $params;

	/* example: ./myfuse --option=value -flag param1 param2 param3	

	/* I will get each argument from console in order */
	/* If I return false parsing is stopped and I should probably set an error */
	public function validate($arg) {
		switch ($arg{0}) {
			case "-": switch($arg{1}){
				case "-": 
					if ($arg = preg_split("~=~", $arg)) {
						$this->options[substr($arg[0], 2)]=$arg[1];	
					}
				break;
				
				default: { $this->flags[] = substr($arg, 1); }
			} break;
			
			default: { $this->params[] = $arg; }
		}

		return true;
	}
}

$my = new My($argv);

var_dump($my);
?>
