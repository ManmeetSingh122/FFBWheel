/* Add this to Serial_SendLive() in serial_cmd.c
   Replace the existing snprintf block with this version
   which adds failsafe_triggered and failsafe_reason fields: */

void Serial_SendLive(void)
{
    char buf[320];
    float angle = ADC_GetWheelAngle();
    uint8_t gear = (uint8_t)ADC_GetGear();
    uint8_t fs   = Failsafe_IsTriggered();

    snprintf(buf, sizeof(buf),
        "{\"type\":\"live\","
        "\"angle\":%.1f,"
        "\"steer_raw\":%u,"
        "\"thr\":%u,\"brake\":%u,\"clutch\":%u,"
        "\"shft_x\":%u,\"shft_y\":%u,"
        "\"gear\":%u,"
        "\"ffb_en\":%u,"
        "\"failsafe\":%u,"
        "\"fs_reason\":\"%s\"}\n",
        (double)angle,
        g_adc[ADC_IDX_STEER],
        g_adc[ADC_IDX_THR],
        g_adc[ADC_IDX_BRAKE],
        g_adc[ADC_IDX_CLUTCH],
        g_adc[ADC_IDX_SHFT_X],
        g_adc[ADC_IDX_SHFT_Y],
        gear,
        g_ffb_enabled,
        fs,
        Failsafe_Reason());
    Serial_Send(buf);
}

/* Also add FAILSAFE_CLEAR command to process_line():

    if (strcmp(line, "FAILSAFE_CLEAR") == 0) {
        Failsafe_Clear();
        Serial_Send("{\"type\":\"ok\",\"msg\":\"Failsafe cleared\"}\n");
        return;
    }
*/
