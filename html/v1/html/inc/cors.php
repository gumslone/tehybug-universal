<?php
// Shared response headers for all config page fragments. The pages are
// fetched cross-origin: the device serves a bootstrap page on
// http://tehybug.local/ whose JS loads these fragments from tehybug.com.
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: POST, GET, OPTIONS");
header('P3P: CP="CAO PSA OUR"'); // Makes IE support cookies
header("Access-Control-Allow-Headers: Origin, X-Requested-With, Content-Type, Accept");
