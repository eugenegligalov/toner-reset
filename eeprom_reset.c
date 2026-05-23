#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>

#define EEPROM_I2C_ADDR (0x53 << 1)

// The payload to flash
const uint8_t dump_bin[] = {
    0x20, 0x00, 0x01, 0x03, 0x01, 0x01, 0x03, 0x00, 0x00, 0x00, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff
};
const uint16_t dump_bin_len = 256;

// Application state structure
typedef struct {
    bool writing;
    bool done;
    bool error;
    uint8_t current_address;
} AppState;

// Render callback for drawing the UI
static void render_callback(Canvas* canvas, void* ctx) {
    AppState* state = ctx;
    canvas_clear(canvas);
    
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignCenter, "EEPROM Resetter");

    canvas_set_font(canvas, FontSecondary);
    if(state->writing) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Writing addr: 0x%02X", state->current_address);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, buf);
    } else if(state->error) {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "I2C Write Failed!");
        canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignCenter, "Check Wiring & Press OK");
    } else if(state->done) {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "Write Successful!");
        canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignCenter, "Press Back to exit");
    } else {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "Connect to Pins 15/16");
        canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignCenter, "Press OK to Start");
    }
}

// Input callback
static void input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

// App Entry Point
int32_t eeprom_reset_main(void* p) {
    UNUSED(p);

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    AppState state = { .writing = false, .done = false, .error = false, .current_address = 0 };

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, &state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    while(1) {
        // Wait for an event
        if(furi_message_queue_get(event_queue, &event, FuriWaitForever) == FuriStatusOk) {
            
            // Exit on Back button
            if(event.type == InputTypeShort && event.key == InputKeyBack) {
                break;
            }

            // Start write on OK button
            if(event.type == InputTypeShort && event.key == InputKeyOk && !state.writing) {
                state.writing = true;
                state.error = false;
                state.done = false;
                view_port_update(view_port);

                // Acquire the external I2C bus (Pins 15/16)
                furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

                for(uint16_t i = 0; i < dump_bin_len; i++) {
                    // ZERO FILTER
                    if(dump_bin[i] == 0x00) continue;

                    state.current_address = i;
                    view_port_update(view_port);

                    // Buffer: [Data Address, Payload]
                    uint8_t tx_buf[2] = { (uint8_t)i, dump_bin[i] };
                    
                    // Send over I2C with 100-tick timeout
                    bool success = furi_hal_i2c_tx(&furi_hal_i2c_handle_external, EEPROM_I2C_ADDR, tx_buf, 2, 100);

                    if(!success) {
                        state.error = true;
                        break;
                    }
                    
                    // 5ms EEPROM write cycle delay
                    furi_delay_ms(5); 
                }

                // Release the bus
                furi_hal_i2c_release(&furi_hal_i2c_handle_external);

                state.writing = false;
                if(!state.error) state.done = true;
                view_port_update(view_port);
            }
        }
    }

    // Cleanup
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);

    return 0;
}