#include <qpdf/Pl_RunLength.hh>

#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

Pl_RunLength::Members::Members(action_e action) :
    action(action),
    state(st_top),
    length(0)
{
}

Pl_RunLength::Members::~Members()
{
}

Pl_RunLength::Pl_RunLength(
    char const* identifier, Pipeline* next, action_e action) :
    Pipeline(identifier, next),
    m(new Members(action))
{
}

Pl_RunLength::~Pl_RunLength()
{
}

void
Pl_RunLength::write(unsigned char* data, size_t len)
{
    if (this->m->action == a_encode) {
        encode(data, len);
    } else {
        decode(data, len);
    }
}

void
Pl_RunLength::encode(unsigned char* data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        if ((this->m->state == st_top) != (this->m->length <= 1)) {
            throw std::logic_error(
                "Pl_RunLength::encode: state/length inconsistency");
        }
        unsigned char ch = data[i];
        if ((this->m->length > 0) &&
            ((this->m->state == st_copying) || (this->m->length < 128)) &&
            (ch == this->m->buf[this->m->length - 1])) {
            QTC::TC(
                "libtests",
                "Pl_RunLength: switch to run",
                (this->m->length == 128) ? 0 : 1);
            if (this->m->state == st_copying) {
                --this->m->length;
                flush_encode();
                this->m->buf[0] = ch;
                this->m->length = 1;
            }
            this->m->state = st_run;
            this->m->buf[this->m->length] = ch;
            ++this->m->length;
        } else {
            if ((this->m->length == 128) || (this->m->state == st_run)) {
                flush_encode();
            } else if (this->m->length > 0) {
                this->m->state = st_copying;
            }
            this->m->buf[this->m->length] = ch;
            ++this->m->length;
        }
    }
}

void
Pl_RunLength::decode(unsigned char* data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        unsigned char ch = data[i];
        switch (this->m->state) {
        case st_top:
            if (ch < 128) {
                // length represents remaining number of bytes to copy
                this->m->length = 1U + ch;
                this->m->state = st_copying;
            } else if (ch > 128) {
                // length represents number of copies of next byte
                this->m->length = 257U - ch;
                this->m->state = st_run;
            } else // ch == 128
            {
                // EOD; stay in this state
            }
            break;

        case st_copying:
            this->getNext()->write(&ch, 1);
            if (--this->m->length == 0) {
                this->m->state = st_top;
            }
            break;

        case st_run:
            for (unsigned int j = 0; j < this->m->length; ++j) {
                this->getNext()->write(&ch, 1);
            }
            this->m->state = st_top;
            break;
        }
    }
}

void
Pl_RunLength::flush_encode()
{
    if (this->m->length == 128) {
        QTC::TC(
            "libtests",
            "Pl_RunLength flush full buffer",
            (this->m->state == st_copying   ? 0
                 : this->m->state == st_run ? 1
                                            : -1));
    }
    if (this->m->length == 0) {
        QTC::TC("libtests", "Pl_RunLength flush empty buffer");
    }
    if (this->m->state == st_run) {
        if ((this->m->length < 2) || (this->m->length > 128)) {
            throw std::logic_error(
                "Pl_RunLength: invalid length in flush_encode for run");
        }
        unsigned char ch = static_cast<unsigned char>(257 - this->m->length);
        this->getNext()->write(&ch, 1);
        this->getNext()->write(&this->m->buf[0], 1);
    } else if (this->m->length > 0) {
        unsigned char ch = static_cast<unsigned char>(this->m->length - 1);
        this->getNext()->write(&ch, 1);
        this->getNext()->write(this->m->buf, this->m->length);
    }
    this->m->state = st_top;
    this->m->length = 0;
}

void
Pl_RunLength::finish()
{
    // When decoding, we might have read a length byte not followed by
    // data, which means the stream was terminated early, but we will
    // just ignore this case since this is the only sensible thing to
    // do.
    if (this->m->action == a_encode) {
        flush_encode();
        unsigned char ch = 128;
        this->getNext()->write(&ch, 1);
    }
    this->getNext()->finish();
}
