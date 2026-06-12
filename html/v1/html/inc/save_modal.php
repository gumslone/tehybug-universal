<?php
// Shared "save config" button plus the restart countdown modal.
// Pages may set before including:
//   $saveButtonLabel - button text (default "Save Config")
//   $saveModalExtra  - extra HTML appended to the modal body
if (!isset($saveButtonLabel)) {
    $saveButtonLabel = 'Save Config';
}
if (!isset($saveModalExtra)) {
    $saveModalExtra = '';
}
?>
<div class="row">
    <div class="col-md-12 my-4">
        <hr>
        <div class="text-center">
            <button type="button" class="btn btn-success shadow save-config-btn" onclick="SaveConfig()" data-bs-toggle="modal" data-bs-target="#popup">
                <span data-feather="save"></span> <?php echo $saveButtonLabel; ?>
            </button>
        </div>
    </div>
</div>

<div class="modal fade" id="popup">
    <div class="modal-dialog modal-dialog-centered modal-lg">
        <div class="modal-content">
            <div class="modal-header bg-light">
                <h3 class="modal-title text-success">Config saved!</h3>
                <button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Close"></button>
            </div>
            <div class="modal-body">
                <div class="alert alert-success">
                    <strong>
                        <span data-feather="refresh-cw"></span> System will restart
                    </strong>
                    <p class="mb-0">Please wait <span id="countdowntimer">12</span> seconds to reload the page.</p>
                </div>
                <?php echo $saveModalExtra; ?>
            </div>
            <div class="modal-footer">
                <button type="button" class="btn btn-secondary" data-bs-dismiss="modal">Close</button>
            </div>
        </div>
    </div>
</div>
