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


#ifndef MO_SIMPLEBLOBTRACKER_MODULE_H
#define MO_SIMPLEBLOBTRACKER_MODULE_H

#include "../moDataGenericContainer.h"
#include "moImageFilterModule.h"
#include "cvaux.h"
#include <iostream>
#include <list>

class moBlob {
friend std::ostream &operator<<(std::ostream &, const moBlob &);

public:
        int id;
        int row;
        int col;
        int width;
        int height;
        int age;
        bool found;

        moBlob();
        moBlob(const moBlob &);
        ~moBlob(){};
        moBlob &operator=(const moBlob &rhs);
        int operator==(const moBlob &rhs) const;
        int operator<(const moBlob &rhs) const;
        int operator>(const moBlob &rhs) const;
};

typedef std::list<moBlob> moBlobList;

class moSimpleBlobTrackerModule : public moImageFilterModule{
public:
        moSimpleBlobTrackerModule();
        virtual ~moSimpleBlobTrackerModule();
        virtual moDataStream *getOutput(int n=0);
	
protected:
        int next_id;
        moBlobList* old_blobs;
        moDataGenericList blobs;
        moDataStream *output_data;

        moBlobList* findItems(IplImage *);
        moBlobList* trackItems(moBlobList *, moBlobList *);
        void drawItems(moBlobList *);
        void applyFilter(IplImage *);
        void allocateBuffers();
        void clearBlobs();

        MODULE_INTERNALS();
};

#endif

