#pragma once

#include "trackbar_base.h"

namespace uih {

/**
 * Track bar implementation, providing standard Windows rendering.
 *
 * \see TrackbarBase
 */
class Trackbar : public TrackbarBase {
protected:
    void get_thumb_rect(unsigned pos, unsigned range, RECT* rc) const override;
    void get_channel_rect(RECT* rc) const override;
    void draw_thumb(HDC dc, const RECT* rc) const override;
    void draw_channel(HDC dc, const RECT* rc) const override;
    void draw_background(HDC dc, const RECT* rc) const override;

    /**
     * Used internally by the standard track bar implementation.\n
     * Used to calculate the height or width of the thumb, depending on the orientation.
     *
     * \return                    Thumb width or height in pixels
     */
    int calculate_thumb_size() const;
};

} // namespace uih
