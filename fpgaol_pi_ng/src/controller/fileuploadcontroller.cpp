#include "controller/fileuploadcontroller.h"

FileUploadController::FileUploadController()
{}

void FileUploadController::service(HttpRequest& request, HttpResponse& response)
{
    QTemporaryFile* file=request.getUploadedFile("bitstream");
    if (file)
    {
        while (!file->atEnd() && !file->error())
        {
            QByteArray buffer=file->read(65536);
        }
        // TODO(programming code)
        response.setStatus(200);
        response.write("Program success!", true);
    }
    else
    {
        response.setStatus(500);
        response.write("upload failed", true);
    }
}
