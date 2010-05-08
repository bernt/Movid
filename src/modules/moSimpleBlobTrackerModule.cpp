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

void moSimpleBlobTrackerModule::applyFilter(IplImage *src) {
        assert(src != NULL);
        CvSize size = cvGetSize(src);
        

        if (src->nChannels != 1) {
                this->setError("BlobTracker input image must be a single channel binary image.");
                this->stop();
                return;
        }

        int minsize = this->property("min_size").asDouble();
        int maxsize = this->property("max_size").asDouble();
        int delta = sqrt(2) * minsize;
        uchar *data = (uchar *)src->imageData;
        bool found;
        int maxcol, mincol, maxrow, minrow, blobwidth, blobheight, blobx, bloby;
        int height = src->height - 1;
        int width = src->width - 1;
        int step = src->widthStep;
        
        cvSet(this->output_buffer, CV_RGB(0,0,0));
        
        // save blobs
        this->old_blobs->Clear();
        delete this->old_blobs;
        this->old_blobs = this->new_blobs;
        this->new_blobs = new CvBlobSeq();
        
        for (int row = minsize; row < height; row += delta) {
                for (int col = minsize; col < width; col +=delta ) {
                        if (data[row * step + col] == 255) {
                                maxcol = col;
                                for (int x = col + 1; x < width; x ++) {
                                        if (data[row * step + x] < 255) {
                                                maxcol = x;
                                                break;
                                        }
                                }
                                blobwidth = maxcol - col;
                                blobx = col + blobwidth / 2;
                                minrow = row;
                                for (int y = row - 1; y > 0; y --) {
                                        if (data[y * step + blobx] < 255) {
                                                minrow = y;
                                                break;
                                        }
                                }
                                maxrow = row;
                                for (int y = row + 1; y < height; y ++) {
                                        if (data[y * step + blobx] < 255) {
                                                maxrow = y;
                                                break;
                                        }
                                }
                                blobheight = maxrow - minrow;
                                bloby = minrow + blobheight / 2;
                                mincol = col;
                                for (int x = col - 1; x > 0; x --) {
                                        if (data[bloby * step + x] < 255) {
                                                mincol = x;
                                                break;
                                        }
                                }
                                found = false;
                                for (int x = maxcol + 1; x < width; x ++) {
                                        if (data[bloby * step + x] < 255) {
                                                if (found) {
                                                        maxcol = x;
                                                }
                                                break;
                                        }
                                        found = true;
                                }
                                if (maxcol - mincol > blobwidth) {
                                        blobwidth = maxcol - mincol;
                                }
                                
                                int row_min = minrow - minsize;
                                if (row_min < 0) {
                                        row_min = 0;
                                }
                                int row_max = maxrow + minsize;
                                if (row_max >= height) {
                                        row_max = height - 1;
                                }
                                
                                int col_min = mincol - minsize;
                                if (col_min < 0) {
                                        col_min = 0;
                                }
                                int col_max = maxcol + minsize;
                                if (col_max >= width) {
                                        col_max = width - 1;
                                }
                                
                                // erase the area of the image containing this blob
                                for (int y = row_min; y <= row_max; y ++) {
                                        for (int x = col_min; x <= col_max; x ++) {
                                                data[y * step + x] = 0;
                                        }
                                }
                                if ((minsize <= blobwidth) && (minsize <= blobheight))
                                {
                                        // look wether we know the id of this blob
                                        LOGM(MO_TRACE) << "Testing x=" << blobx << " y=" << bloby;
                                        CvBlob *pB = new CvBlob;
                                        pB->ID = 0;
                                        for (int nr = this->old_blobs->GetBlobNum(); nr > 0; nr --)
                                        {
                                                CvBlob* pO = this->old_blobs->GetBlob(nr);
                                                if ((pO->x - pO->w / 2 <= blobx)  && (blobx <= pO->x + pO->w / 2) && (pO->y - pO->h / 2 <= bloby) && (bloby <= pO->y + pO->h / 2))
                                                {
                                                        LOGM(MO_TRACE) << "Found ID:" << pO->ID << " x=" << pO->x - pO->w / 2 << "--" << pO->x + pO->w / 2 << " y=" << pO->y - pO->h / 2 << "--" << pO->y + pO->h / 2;
                                                        if (pB->ID == 0) {
                                                                pB->ID = pO->ID;
                                                        }
                                                        this->old_blobs->DelBlob(nr);
                                                }
                                        }
                                        // look from the other point of view
                                        for (int nr = this->old_blobs->GetBlobNum(); nr > 0; nr --)
                                        {
                                                CvBlob *pO = this->old_blobs->GetBlob(nr);
                                                if ((pO->x - blobwidth / 2 <= blobx)  && (blobx <= pO->x + blobwidth / 2) && (pO->y -  blobheight / 2 <= bloby) && (bloby <= pO->y + blobheight / 2))
                                                {
                                                        LOGM(MO_TRACE) << "2. Try found ID:" << pO->ID << " x=" << pO->x - blobwidth / 2 << "--" << pO->x + blobwidth / 2 << " y=" << pO->y - blobheight / 2 << "--" << pO->y + blobheight / 2;
                                                        if (pB->ID == 0) {
                                                                pB->ID = pO->ID;
                                                        }
                                                        this->old_blobs->DelBlob(nr);
                                                }
                                        }
                                        // check if this blob has a collision with a blob already detected
                                        found = false;
                                        for (int nr = this->new_blobs->GetBlobNum(); nr > 0; nr --)
                                        {
                                                CvBlob *pX = this->new_blobs->GetBlob(nr);
                                                if ((pX->x - pX->w / 2 <= blobx)  && (blobx <= pX->x + pX->w / 2) && (pX->y - pX->h / 2 <= bloby) && (bloby <= pX->y + pX->h / 2))
                                                {
                                                        LOGM(MO_TRACE) << "Collision ID:" << pX->ID << " x=" << pX->x - pX->w / 2 << "--" << pX->x + pX->w / 2 << " y=" << pX->y - pX->h / 2 << "--" << pX->y + pX->h / 2;
                                                        found = true;
                                                        break;
                                                }
                                        }
                                        if (~found)
                                        {
                                                if (pB->ID == 0)
                                                {
                                                        pB->ID = this->next_id;
                                                        this->next_id ++;
                                                }
                                                pB->x = blobx;
                                                pB->y = bloby;
                                                pB->w =blobwidth ;
                                                pB->h = blobheight;
                                                this->new_blobs->AddBlob(pB);
                                        }
                                }
                                col = maxcol;
                        }
                }
        }

        // decrease width and high of each undetected old blob
        for (int nr = old_blobs->GetBlobNum(); nr > 0; nr --)
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
        for (int nr = this->old_blobs->GetBlobNum(); nr > 0; nr --)
        {
                this->new_blobs->AddBlob(this->old_blobs->GetBlob(nr));
        }

        this->clearBlobs();
        for (int i = this->new_blobs->GetBlobNum() ; i > 0; i --) {
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



