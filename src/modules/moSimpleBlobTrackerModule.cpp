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

moBlob::moBlob()
{
        id = 0;
        row = 0;
        col = 0;
        width = 0;
        height = 0;
        age = 0;
        found = false;
}

moBlob::moBlob(const moBlob &copyin) // Copy constructor to handle pass by value.
{
        id = copyin.id;
        row = copyin.row;
        col = copyin.col;
        width = copyin.width;
        height = copyin.height;
        age = copyin.age;
        found = copyin.found;
}

std::ostream &operator<<(std::ostream &output, const moBlob &blob)
{
        output << blob.id << ' ' << blob.row << ' ' << blob.col << ' ' << blob.width << ' ' << blob.height << ' ' << blob.age << std::endl;
        return output;
}

moBlob& moBlob::operator=(const moBlob &rhs)
{
        this->id = rhs.id;
        this->row = rhs.row;
        this->col = rhs.col;
        this->width = rhs.width;
        this->height = rhs.height;
        this->age = rhs.age;
        this->found = rhs.found;
        return *this;
}

int moBlob::operator==(const moBlob &rhs) const
{
        if( this->row != rhs.row)
                return 0;
        if( this->col != rhs.col)
                return 0;
        if( this->width != rhs.width)
                return 0;
        if( this->height != rhs.height)
                return 0;
        if( this->age != rhs.age)
                return 0;
        if( this->found != rhs.found)
                return 0;
        return 1;
}

// This function is required for built-in STL list functions like sort
int moBlob::operator<(const moBlob &rhs) const
{
        if( this->row == rhs.row && this->col == rhs.col && this->width == rhs.width && this->height == rhs.height && this->age < rhs.age)
                return 1;
        if( this->row == rhs.row && this->col == rhs.col && this->width == rhs.width && this->height < rhs.height)
                return 1;
        if( this->row == rhs.row && this->col == rhs.col && this->width < rhs.width)
                return 1;
        if( this->row == rhs.row && this->col < rhs.col)
                return 1;
        if( this->row < rhs.row)
                return 1;
        return 0;
}

int moBlob::operator>(const moBlob &rhs) const
{
        if( this->row == rhs.row && this->col == rhs.col && this->width == rhs.width && this->height == rhs.height && this->age > rhs.age)
                return 1;
        if( this->row == rhs.row && this->col == rhs.col && this->width == rhs.width && this->height > rhs.height)
                return 1;
        if( this->row == rhs.row && this->col == rhs.col && this->width > rhs.width)
                return 1;
        if( this->row == rhs.row && this->col > rhs.col)
                return 1;
        if( this->row > rhs.row)
                return 1;
        return 0;
}

moSimpleBlobTrackerModule::moSimpleBlobTrackerModule() : moImageFilterModule(){

        MODULE_INIT();

        // Minimum and maximum sizes for blobs. Blobs smaller or larger
        // will be discarded.
        this->properties["min_size"] = new moProperty(8.0);
        this->properties["max_size"] = new moProperty(25.0);

        this->output_data = new moDataStream("GenericTouch");
        this->output_count = 2;
        this->output_infos[1] = new moDataStreamInfo("data", "GenericTouch", "Data stream with touch info");

        this->old_blobs = new moBlobList;
        this->next_id = 1;
}

moSimpleBlobTrackerModule::~moSimpleBlobTrackerModule() {
        this->clearBlobs();
        delete this->output_data;
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
        LOGM(MO_TRACE, "allocated output buffer for BlobTracker module."); 
}

moBlobList* moSimpleBlobTrackerModule::findItems(IplImage *src) {
        // find pixels which could belong to blobs, but dont test every pixels in the image
        bool found;
        int maxcol, mincol, maxrow, minrow, blobwidth, blobheight, blobx, bloby;
        int minsize = this->property("min_size").asDouble();
        int maxsize = this->property("max_size").asDouble();
        int delta = sqrt(2) * minsize;
        int height = src->height - 1;
        int width = src->width - 1;
        int step = src->widthStep;
        uchar *data = (uchar *)src->imageData;
        moBlobList* bloblist = new moBlobList;
        moBlob blob;

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
                                
                                // erase the area of the image containing this blob, so we don't detect this blob twice
                                for (int y = row_min; y <= row_max; y ++) {
                                        for (int x = col_min; x <= col_max; x ++) {
                                                data[y * step + x] = 0;
                                        }
                                }
                                if ((minsize <= blobwidth) && (minsize <= blobheight) && (blobwidth <= maxsize) && (blobheight <= maxsize))
                                {
                                        LOGM(MO_TRACE, "Found x=" << blobx << " y=" << bloby);
                                        blob.row = blobx;
                                        blob.col = bloby;
                                        blob.width = blobwidth;
                                        blob.height = blobheight;
                                        blob.age = 0;
                                        blob.found = false;
                                        // check for duplicates
                                        for (moBlobList::iterator old = bloblist->begin(); old != bloblist->end(); old ++) {
                                                if (((old->row - old->height / 2 < blob.row) && (blob.row < old->row + old->height / 2)
                                                        && (old->col - old->width / 2 < blob.col) && (blob.col < old->col + old->width / 2))
                                                        || ((blob.row - blob.height / 2 < old->row) && (old->row < blob.row + blob.height / 2)
                                                        && (blob.col - blob.width / 2 < old->col) && (old->col < blob.col + blob.width / 2))) {
                                                        blob.found = true;
                                                        minrow = MIN(old->row - old->height / 2 , blob.row - blob.height / 2);
                                                        maxrow = MAX(old->row + old->height / 2 , blob.row + blob.height / 2);
                                                        mincol = MIN(old->col - old->width / 2, blob.col - blob.width / 2);
                                                        maxcol = MAX(old->col + old->width / 2, blob.col + blob.width / 2);
                                                        old->row = (minrow + maxrow) / 2;
                                                        old->col  = (mincol + maxcol) / 2;
                                                        old->height = maxrow - minrow;
                                                        old->width = maxcol - mincol;
                                                }
                                        }
                                        if (~blob.found) {
                                                bloblist->push_back(blob);
                                        }
                                }
                                col = maxcol;
                        }
                }
        }
        return bloblist;
}

moBlobList* moSimpleBlobTrackerModule::trackItems(moBlobList *bloblist, moBlobList *oldlist) {
        moBlobList *newlist = new moBlobList;
        int min_age = this->property("min_age").asInteger();
        int max_age = this->property("max_age").asInteger();
        for (moBlobList::iterator blob = oldlist->begin(); blob != oldlist->end(); blob ++) {
                blob->found = false;
        }
        for (moBlobList::iterator blob = bloblist->begin(); blob != bloblist->end(); blob ++) {
                blob->found = false;
                for (moBlobList::iterator old = oldlist->begin(); old != oldlist->end(); old ++) {
                        if (((old->row - old->height / 2 < blob->row) && (blob->row < old->row + old->height / 2) && (old->col - old->width / 2 < blob->col) && (blob->col < old->col + old->width / 2))
                                || ((blob->row - blob->height / 2 < old->row) && (old->row < blob->row + blob->height / 2) && (blob->col - blob->width / 2 < old->col) && (old->col < blob->col + blob->width / 2))) {
                                blob->found = true;
                                old->found = true;
                                blob->id = old->id;
                                if (old->age < min_age) {
                                        old->age ++;
                                        if (blob->age < old->age) {
                                                blob->age = old->age;
                                        }
                                }
                        }
                }
                if (!(blob->found)) {
                        blob->id = this->next_id;
                        this->next_id ++;
                        blob->found = true;
                }
                newlist->push_back(*blob);
        }
        for (moBlobList::iterator old = oldlist->begin(); old != oldlist->end(); old ++) {
                if (!(old->found)) {
                        old->age ++;
                        if (old->age <= max_age) {
                                old->found = true;
                                newlist->push_back(*old);
                        }
                }
        }
        return newlist;
}

void moSimpleBlobTrackerModule::drawItems(moBlobList *bloblist) {
        CvFont font;
        char text[20] = "MMMM";
        cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 1.0, 1.0, 0, 1, CV_AA);
        cvSet(this->output_buffer, cvScalar(0, 0, 0));
        for (moBlobList::iterator blob = bloblist->begin(); blob != bloblist->end(); blob ++) {
                CvPoint p = cvPoint(cvRound(blob->col*256),cvRound(blob->row*256));
                CvSize  s = cvSize(MAX(1, cvRound(blob->width * 128)), MAX(1, cvRound(blob->height * 128)));
                cvEllipse(this->output_buffer, p, s, 0, 0, 360, CV_RGB(0, 255, 0), cvRound(1 + (3 * 0) / 255), CV_AA, 8);
                sprintf(text, "%i", blob->id);
                cvPutText(this->output_buffer, text, cvPoint(blob->col, blob->row), &font, cvScalar(255, 255, 255, 0));
        }
}

void moSimpleBlobTrackerModule::sendOutput(IplImage *src, moBlobList *bloblist) {
        CvSize size = cvGetSize(src);
        this->clearBlobs();
        for (moBlobList::iterator blob = bloblist->begin(); blob != bloblist->end(); blob ++) {
                moDataGenericContainer *touch = new moDataGenericContainer();
                
                touch->properties["type"] = new moProperty("touch");
                touch->properties["id"] = new moProperty(blob->id);
                touch->properties["x"] = new moProperty((float)blob->col / size.width);
                touch->properties["y"] = new moProperty((float)blob->row / size.height);
                touch->properties["w"] = new moProperty(blob->width);
                touch->properties["h"] = new moProperty(blob->height);
                //std::cout << blob->id << ": x=" << (float)blob->col / size.width << " y=" << (float)blob->row / size.height << std::endl;
                this->blobs.push_back(touch);
        }
        this->output_data->push(&this->blobs);
}

void moSimpleBlobTrackerModule::applyFilter(IplImage *src) {
        moBlobList * tracked_blobs;
        moBlobList * new_blobs;
        assert(src != NULL);

        if (src->nChannels != 1) {
                this->setError("BlobTracker input image must be a single channel binary image.");
                this->stop();
                return;
        }
        
        new_blobs = findItems(src);
        tracked_blobs = trackItems(new_blobs, old_blobs);
        drawItems(tracked_blobs);
        sendOutput(src, tracked_blobs);
        delete this->old_blobs;
        delete new_blobs;
        old_blobs = tracked_blobs;
}

moDataStream* moSimpleBlobTrackerModule::getOutput(int n) {
        if (n == 1) {
                return this->output_data;
        }
        return moImageFilterModule::getOutput(n);
}



