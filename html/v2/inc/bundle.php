<?php
/**
 * Asset bundler shared by css/style.php and js/javascript.php.
 *
 * Concatenates the files listed in ../manifest.json (one source of truth,
 * also read by tools/mock-device.js) in order, gzips when the client
 * accepts it, and revalidates on every load with an ETag built from the
 * files' mtimes and sizes: the device page loads these bundles without a
 * version parameter, so a fixed max-age kept serving stale code for its
 * whole lifetime after every deploy. no-cache + ETag makes each load a
 * cheap 304 until something actually changes.
 *
 * Plain PHP 5 syntax on purpose: the web server runs an older PHP than
 * the command line does (type declarations made it die with a parse error).
 */
function serveBundle($kind, $contentType, $separator)
{
    $manifest = json_decode((string)file_get_contents(__DIR__ . '/../manifest.json'), true);
    $files = array();
    if (isset($manifest[$kind]) && is_array($manifest[$kind])) {
        foreach ($manifest[$kind] as $relative) {
            $files[] = __DIR__ . '/../' . $kind . '/' . $relative;
        }
    }

    $etagSource = '';
    foreach ($files as $file) {
        $etagSource .= $file . ':' . (string)@filemtime($file) . ':' . (string)@filesize($file) . ';';
    }
    $etag = '"' . md5($etagSource) . '"';

    header('Content-Type: ' . $contentType);
    header('Cache-Control: no-cache');
    header('ETag: ' . $etag);
    // The bundles are fetched from the device's own page (http://tehybug.local/),
    // a different origin from tehybug.com.
    header('Access-Control-Allow-Origin: *');

    if (isset($_SERVER['HTTP_IF_NONE_MATCH']) && trim($_SERVER['HTTP_IF_NONE_MATCH']) === $etag) {
        header('HTTP/1.1 304 Not Modified');
        exit;
    }

    if (isset($_SERVER['HTTP_ACCEPT_ENCODING']) && strpos($_SERVER['HTTP_ACCEPT_ENCODING'], 'gzip') !== false) {
        header('Content-Encoding: gzip');
        header('Vary: Accept-Encoding');
        ob_start('ob_gzhandler');
    } else {
        ob_start();
    }

    foreach ($files as $file) {
        if (file_exists($file)) {
            readfile($file);
            // A file may end without a newline or, for JS, in a line comment;
            // the separator keeps it from swallowing the next file's first line.
            echo $separator;
        }
    }

    ob_end_flush();
}
