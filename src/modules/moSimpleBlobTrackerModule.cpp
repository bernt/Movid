/***********************************************************************
 ** Copyright (C) 2010 Movid Authors.  All rights reserved.
 **
 ** This file is part of the Movid Software.
 **
 ** This file may be distributed under the terms of the Q Public License
 ** as defined by Trolltech AS of Norway and appearing in the file
 ** LICENSE included in the packaging of this file.
 **
 ** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 ** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 **
 ** Contact info@movid.org if any conditions of this licensing are
 ** not clear to you.
 **
 **********************************************************************/


#include <assert.h>
#include "moSimpleBlobTrackerModule.h"
#include "../moLog.h"
#include "cv.h"
#include "cvaux.h"

MODULE_DECLARE(SimpleBlobTracker, "native", "Tracks Blobs in a simple way");

moSimpleBlobTrackerModule::moSimpleBlobTrackerModule() : moImageFilterModule(){

	MODULE_INIT();

    // Minimum and maximum sizes for blobs. Blobs smaller or larger
    // will be discarded.
    this->properties["min_size"] = new moProperty(8.0);
    this->properties["max_size"] = new moProperty(25.0);

    this->output_data = new moDataStream("GenericTouch");
    this->output_count = 2;
    this->output_infos[1] = new moDataStreamInfo("data", "GenericTouch", "Data stream with touch info");

    this->next_id = 1;
    this->new_blobs = new CvBlobSeq();
    this->old_blobs = new CvBlobSeq();
}

moSimpleBlobTrackerModule::~moSimpleBlobTrackerModule() {
    this->clearBlobs();
    delete this->output_data;
    delete this->new_blobs;
    delete this->old_blobs;
}

void moSimpleBlobTrackerModule::clearBlobs() {
    moDataGenericList::iterator it;
    for (it = this->blobs.begin(); it != this->blobs.end(); it ++) {
        delete (*it);
    }
    this->blobs.clear();
}

void moSimpleBlobTrackerModule::allocateBuffers() {
    IplImage* src = static_cast<IplImage*>(this->input->getData());
    if (src == NULL) {
        return;
    }
    this->output_buffer = cvCreateImage(cvGetSize(src), src->depth, 3);
    LOGM(MO_TRACE) << "allocated output buffer for BlobTracker module.";
}

void moSimpleBlobTrackerModule::applyFilter() {
        IplImage* src = static_cast<IplImage*>(this->input->getData());
        assert(src != NULL);
        CvSize size = cvGetSize(src);
        

        if (src->nChannels != 1) {
                this->setError("BlobTracker input image must be a single channel binary image.");
                this->stop();
                return;
        }

        int minsize = this->property("min_size").asDouble();
        int maxsize = this->property("max_size").asDouble();
        
        cvSet(this->output_buffer, CV_RGB(0,0,0));

        uchar *data = (uchar *)src->imageData;
        int i, j, col, row, w, h, col0, row_n, col_n, row0, col1, w1, x, y, nr;
        int bFound = 0;
        int height = src->height - 1;
        int width = src->width - 1;
        int step = src->widthStep;
        this->old_blobs->Clear();
        for(i=0; i < this->new_blobs->GetBlobNum(); i ++)
        {
                CvBlob* pBN = this->new_blobs->GetBlob(i);
                this->old_blobs->AddBlob(pBN);
        }
        this->new_blobs->Clear();
        
        for (i = minsize; i < height; i += minsize) {
                for (j = minsize; j < width; j +=minsize ) {
                        if (data[i * step + j] == 255) {
                                bFound = 1;
                                for (col = j + 1; col < width; col ++) {
                                        if (data[i * step + col] < 255) {
                                                break;
                                        }
                                }
                                w = col - j;
                                col0 = j + w/2;
                                for (row = i  - 1; row > 0; row --) {
                                        if (data[row * step + col0] < 255) {
                                                break;
                                        }
                                }
                                row_n = row + 1;
                                for (row = i + 1; row < height; row ++) {
                                        if (data[row * step + col0] < 255) {
                                                break;
                                        }
                                }
                                h = row - row_n;
                                row0 = row_n + h/2;
                                for (col1 = j - 1; col1 > 0; col1 --) {
                                        if (data[row0 * step + col1] < 255) {
                                                break;
                                        }
                                }
                                col_n = col1 + 1;
                                for (col1 = j + 1; col1 < width; col1 ++) {
                                        if (data[row0 * step + col1] < 255) {
                                                break;
                                        }
                                }
                                w1 = col1 - col_n;
                                
                                int row_min = row_n - minsize;
                                if (row_min < 0) {
                                        row_min = 0;
                                }
                                int row_max = row_n + h + minsize;
                                if (row_max >= height) {
                                        row_max = height - 1;
                                }
                                
                                int col_min = col_n - minsize;
                                if (col_min < 0) {
                                        col_min = 0;
                                }
                                int col_max = col_n + w1 + minsize;
                                if (col_max >= width) {
                                        col_max = width - 1;
                                }
                                
                                // erase the area of the image containing this blob
                                for (y = row_min; y <= row_max; y ++) {
                                        for (x = col_min; x <= col_max; x ++) {
                                                data[y * step + x] = 0;
                                        }
                                }
                                col0 = col_n + w1/2;
                                if ((minsize <= w1) && (minsize <= h))
                                {
                                        // look wether we know the id of this blob
                                        LOGM(MO_TRACE) << "Testing x=" << col0 << " y=" << row0;
                                        CvBlob *pB = new CvBlob;
                                        pB->ID = 0;
                                        for (nr = this->old_blobs->GetBlobNum(); nr > 0; nr --)
                                        {
                                                CvBlob* pO = this->old_blobs->GetBlob(nr);
                                                if ((pO->x - pO->w / 2 <= col0)  && (col0 <= pO->x + pO->w / 2) && (pO->y - pO->h / 2 <= row0) && (row0 <= pO->y + pO->h / 2))
                                                {
                                                        LOGM(MO_TRACE) << "Found ID:" << pO->ID << " x=" << pO->x - pO->w / 2 << "--" << pO->x + pO->w / 2 << " y=" << pO->y - pO->h / 2 << "--" << pO->y + pO->h / 2;
                                                        if (pB->ID == 0) {
                                                                pB->ID = pO->ID;
                                                        }
                                                        this->old_blobs->DelBlob(nr);
                                                }
                                        }
                                        // look from the other point of view
                                        for (nr = this->old_blobs->GetBlobNum(); nr > 0; nr --)
                                        {
                                                CvBlob *pO = this->old_blobs->GetBlob(nr);
                                                if ((pO->x - w1 / 2 <= col0)  && (col0 <= pO->x + w1 / 2) && (pO->y - h / 2 <= row0) && (row0 <= pO->y + h / 2))
                                                {
                                                        LOGM(MO_TRACE) << "2. Try found ID:" << pO->ID << " x=" << pO->x - w1 / 2 << "--" << pO->x + w1 / 2 << " y=" << pO->y - h / 2 << "--" << pO->y + h / 2;
                                                        if (pB->ID == 0) {
                                                                pB->ID = pO->ID;
                                                        }
                                                        this->old_blobs->DelBlob(nr);
                                                }
                                        }
                                        // check if this blob has a collision with a blob already detected
                                        int bCollision = 0;
                                        for (nr = this->new_blobs->GetBlobNum(); nr > 0; nr --)
                                        {
                                                CvBlob *pX = this->new_blobs->GetBlob(nr);
                                                if ((pX->x - pX->w / 2 <= col0)  && (col0 <= pX->x + pX->w / 2) && (pX->y - pX->h / 2 <= row0) && (row0 <= pX->y + pX->h / 2))
                                                {
                                                        LOGM(MO_TRACE) << "Collision ID:" << pX->ID << " x=" << pX->x - pX->w / 2 << "--" << pX->x + pX->w / 2 << " y=" << pX->y - pX->h / 2 << "--" << pX->y + pX->h / 2;
                                                        bCollision = 1;
                                                        break;
                                                }
                                        }
                                        if (bCollision == 0)
                                        {
                                                if (pB->ID == 0)
                                                {
                                                        pB->ID = this->next_id;
                                                        this->next_id ++;
                                                }
                                                pB->x = col0;
                                                pB->y = row0;
                                                pB->w = w1;
                                                pB->h = h;
                                                this->new_blobs->AddBlob(pB);
                                        }
                                }
                                j = col;
                        }
                }
        }

        // decrease width and high of each undetected old blob
        for (nr = old_blobs->GetBlobNum(); nr > 0; nr --)
        {
                CvBlob *pO = this->old_blobs->GetBlob(nr);
                pO->h --;
                pO->w --;
                if ((pO->h <= 0) || (pO->w <= 0))
                {
                        this->old_blobs->DelBlob(nr);
                }
        }
        // add the rest
        for (nr = this->old_blobs->GetBlobNum(); nr > 0; nr --)
        {
                this->new_blobs->AddBlob(this->old_blobs->GetBlob(nr));
        }

        this->clearBlobs();
        for (i = this->new_blobs->GetBlobNum() ; i > 0; i --) {
                CvBlob* pB = this->new_blobs->GetBlob(i);
                LOGM(MO_TRACE) << i << ": ID=" << pB->ID << " x=" << pB->x << " y=" << pB->y;
                // Assume circular blobs
                if (pB->w < minsize || maxsize < pB->w || pB->h < minsize || maxsize < pB->h) {
                        continue;
                }
                // draw the blob on output image
                if (this->output->getObserverCount() > 0) {
                        CvPoint p = cvPoint(cvRound(pB->x * 256), cvRound(pB->y * 256));
                        CvSize  s = cvSize(MAX(1, cvRound(CV_BLOB_RX(pB) * 256)), MAX(1, cvRound(CV_BLOB_RY(pB) * 256)));
                        int c = 0; //cvRound(255*this->tracker->GetState(CV_BLOB_ID(pB)));

                        cvEllipse(this->output_buffer, p, s, 0, 0, 360,
                        CV_RGB(c, 255 - c, 0), cvRound(1 + (3 * 0) / 255), CV_AA, 8);
                }

                // add the blob in data
                moDataGenericContainer *touch = new moDataGenericContainer();
                touch->properties["type"] = new moProperty("touch");
                touch->properties["id"] = new moProperty(pB->ID);
                touch->properties["x"] = new moProperty(pB->x / size.width);
                touch->properties["y"] = new moProperty(pB->y / size.height);
                touch->properties["w"] = new moProperty(pB->w);
                touch->properties["h"] = new moProperty(pB->h);
                this->blobs.push_back(touch);
        }
        this->output_data->push(&this->blobs);
        LOGM(MO_TRACE) << "#### done ###";
}

moDataStream* moSimpleBlobTrackerModule::getOutput(int n) {
        if (n == 1) {
                return this->output_data;
        }
        return moImageFilterModule::getOutput(n);
}



