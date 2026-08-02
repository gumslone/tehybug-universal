<?php
/**
 * JavaScript Asset Bundler
 *
 * This script concatenates multiple JavaScript files into a single response,
 * handles GZIP compression, and sets appropriate caching headers.
 */

// --- Configuration ---

// Define the list of JavaScript files to include.
// The order is important as files are concatenated sequentially.
$jsFiles = [
    './files/bootstrap.min.js',
    './files/jquery-3.6.0.min.js',
    './files/feather.min.js',
    './files/gumboard.js',
];

// --- Asset Delivery ---
//
// Revalidate on every load instead of a fixed-lifetime cache: the device
// pages load this bundle with no version parameter, so a max-age cache kept
// serving stale scripts for its full lifetime after every deploy. An ETag
// from the bundled files' mtimes makes the browser ask each time and get a
// cheap 304 whenever nothing changed - fresh after deploys, near-free
// otherwise.
$etagSource = '';
foreach ($jsFiles as $f) {
    $etagSource .= $f . ':' . (string)@filemtime($f) . ';';
}
$etag = '"' . md5($etagSource) . '"';

header("Content-type: text/javascript; charset=utf-8");
header("Cache-Control: no-cache");
header("ETag: " . $etag);

if (isset($_SERVER['HTTP_IF_NONE_MATCH']) &&
    trim($_SERVER['HTTP_IF_NONE_MATCH']) === $etag) {
    http_response_code(304);
    exit;
}

// Enable GZIP compression if supported by the client.
if (isset($_SERVER['HTTP_ACCEPT_ENCODING']) && strpos($_SERVER['HTTP_ACCEPT_ENCODING'], 'gzip') !== false) {
    header("Content-Encoding: gzip");
    header("Vary: Accept-Encoding");
    ob_start('ob_gzhandler');
} else {
    ob_start();
}

// Concatenate and output the contents of the JavaScript files.
foreach ($jsFiles as $file) {
    if (file_exists($file)) {
        readfile($file);
        // Separate files with a newline plus an empty statement: a minified
        // file may end in an unterminated line comment (e.g. a sourceMappingURL
        // marker) or lack a trailing semicolon, which would corrupt the next one.
        echo "\n;\n";
    }
}

ob_end_flush();