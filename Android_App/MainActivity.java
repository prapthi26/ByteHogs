package com.hummerai.robot;

import android.app.Activity;
import android.os.Bundle;
import android.hardware.usb.UsbManager;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.content.Context;
import android.content.Intent;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.IntentFilter;
import android.widget.*;
import android.view.View;
import android.view.MotionEvent;
import android.graphics.Color;
import android.widget.LinearLayout;

import com.hoho.android.usbserial.driver.*;
import com.hoho.android.usbserial.util.SerialInputOutputManager;

import java.util.List;
import java.util.concurrent.Executors;

public class MainActivity extends Activity
        implements SerialInputOutputManager.Listener {

    private UsbManager usbManager;
    private UsbSerialPort port;
    private SerialInputOutputManager ioManager;

    private TextView status, distance, sensors;
    private SeekBar speedBar;

    private static final String ACTION_USB_PERMISSION =
            "com.hummerai.robot.USB_PERMISSION";

    private void show(String message) {
        Toast.makeText(this, message,
                Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        usbManager = (UsbManager)
                getSystemService(Context.USB_SERVICE);

        buildDashboard();

    }

    private void buildDashboard() {

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(24, 24, 24, 24);
        root.setBackgroundColor(Color.rgb(18, 28, 45));

        TextView title = new TextView(this);
        title.setText("HUMMER AI");
        title.setTextSize(30);
        title.setTextColor(Color.WHITE);
        root.addView(title);

        status = new TextView(this);
        status.setText("USB: DISCONNECTED");
        status.setTextColor(Color.YELLOW);
        root.addView(status);

        Button connect = makeButton("CONNECT ROBOT");
        connect.setOnClickListener(v -> connectUSB());
        root.addView(connect);

        TextView heading = new TextView(this);
        heading.setText("MANUAL CONTROL");
        heading.setTextColor(Color.WHITE);
        heading.setTextSize(20);
        root.addView(heading);

        addControl(root, "FORWARD", 'F');
        addControl(root, "LEFT", 'L');
        addControl(root, "STOP", 'S');
        addControl(root, "RIGHT", 'R');
        addControl(root, "BACKWARD", 'B');

        Button auto = makeButton("AUTONOMOUS MODE");
        auto.setOnClickListener(v -> send('A'));
        root.addView(auto);

        Button patrol = makeButton("PATROL MODE");
        patrol.setOnClickListener(v -> send('P'));
        root.addView(patrol);

        Button manual = makeButton("MANUAL MODE");
        manual.setOnClickListener(v -> send('M'));
        root.addView(manual);

        Button emergency = makeButton("EMERGENCY STOP");
        emergency.setBackgroundColor(Color.RED);
        emergency.setOnClickListener(v -> send('E'));
        root.addView(emergency);

        TextView speedTitle = new TextView(this);
        speedTitle.setText("MOTOR SPEED");
        speedTitle.setTextColor(Color.WHITE);
        root.addView(speedTitle);

        speedBar = new SeekBar(this);
        speedBar.setMax(9);
        speedBar.setProgress(6);

        speedBar.setOnSeekBarChangeListener(
            new SeekBar.OnSeekBarChangeListener() {

                public void onProgressChanged(
                    SeekBar bar, int value, boolean user
                ) {
                    if (user && port != null) {
                        send((char)('0' + value));
                    }
                }

                public void onStartTrackingTouch(SeekBar b) {}
                public void onStopTrackingTouch(SeekBar b) {}
            }
        );

        root.addView(speedBar);

        distance = new TextView(this);
        distance.setText("Distance: -- cm");
        distance.setTextColor(Color.WHITE);
        root.addView(distance);

        sensors = new TextView(this);
        sensors.setText("IR Sensors: Waiting");
        sensors.setTextColor(Color.WHITE);
        root.addView(sensors);

        setContentView(root);

    }

    private Button makeButton(String text) {
        Button b = new Button(this);
        b.setText(text);
        return b;
    }

    private void addControl(
        LinearLayout root, String label, char command
    ) {

        Button b = makeButton(label);

        b.setOnTouchListener((view, event) -> {

            if (event.getAction() ==
                    MotionEvent.ACTION_DOWN) {

                send(command);
                return true;

            }

            if (event.getAction() ==
                    MotionEvent.ACTION_UP ||
                event.getAction() ==
                    MotionEvent.ACTION_CANCEL) {

                send('S');
                return true;

            }

            return true;

        });

        root.addView(b);

    }

    private void connectUSB() {

        try {

            List<UsbSerialDriver> drivers =
                UsbSerialProber.getDefaultProber()
                .findAllDrivers(usbManager);

            if (drivers.isEmpty()) {
                show("Arduino not detected");
                return;
            }

            UsbSerialDriver driver = drivers.get(0);
            UsbDevice device = driver.getDevice();

            if (!usbManager.hasPermission(device)) {

                PendingIntent permissionIntent =
                    PendingIntent.getBroadcast(
                        this,
                        0,
                        new Intent(ACTION_USB_PERMISSION)
                            .setPackage(getPackageName()),
                        PendingIntent.FLAG_IMMUTABLE
                    );

                IntentFilter filter =
                    new IntentFilter(ACTION_USB_PERMISSION);

                registerReceiver(
                    permissionReceiver, filter,
                    Context.RECEIVER_NOT_EXPORTED
                );

                usbManager.requestPermission(
                    device, permissionIntent
                );

                return;

            }

            openSerial(driver);

        } catch (Exception e) {

            show("USB error: " + e.getMessage());

        }

    }

    private final BroadcastReceiver permissionReceiver =
        new BroadcastReceiver() {

            @Override
            public void onReceive(
                Context context, Intent intent
            ) {

                if (ACTION_USB_PERMISSION.equals(
                        intent.getAction())) {

                    if (intent.getBooleanExtra(
                        UsbManager.EXTRA_PERMISSION_GRANTED,
                        false)) {

                        connectUSB();

                    } else {
                        show("USB permission denied");
                    }

                }

            }

        };

    private void openSerial(UsbSerialDriver driver) {

        try {

            UsbDeviceConnection connection =
                usbManager.openDevice(driver.getDevice());

            if (connection == null) {
                show("Unable to open USB device");
                return;
            }

            port = driver.getPorts().get(0);
            port.open(connection);

            port.setParameters(
                9600,
                8,
                UsbSerialPort.STOPBITS_1,
                UsbSerialPort.PARITY_NONE
            );

            ioManager =
                new SerialInputOutputManager(port, this);

            ioManager.start();

            runOnUiThread(() ->
                status.setText("USB: CONNECTED")
            );

            show("Hummer Bot connected");

        } catch (Exception e) {

            show("Connection error: " + e.getMessage());

        }

    }

    private void send(char command) {

        if (port == null) return;

        try {

            port.write(
                new byte[]{(byte) command},
                100
            );

        } catch (Exception e) {

            runOnUiThread(() ->
                status.setText("USB: CONNECTION ERROR")
            );

        }

    }

    @Override
    public void onNewData(byte[] data) {

        String message = new String(data);

        runOnUiThread(() -> {

            for (String line : message.split("[\\r\\n]+")) {

                if (!line.startsWith("DATA,")) continue;

                String[] parts = line.split(",");

                if (parts.length >= 6) {

                    distance.setText(
                        "Distance: " + parts[2] + " cm"
                    );

                    sensors.setText(
                        "Left IR: " + parts[3] +
                        " | Right IR: " + parts[4] +
                        " | Speed: " + parts[5]
                    );

                    status.setText(
                        "USB CONNECTED | MODE: " + parts[1]
                    );

                }

            }

        });

    }

    @Override
    public void onRunError(Exception e) {

        runOnUiThread(() ->
            status.setText("USB: DISCONNECTED")
        );

    }

    @Override
    protected void onDestroy() {

        send('E');

        if (ioManager != null) {
            ioManager.stop();
        }

        try {
            if (port != null) port.close();
        } catch (Exception ignored) {}

        super.onDestroy();

    }

}