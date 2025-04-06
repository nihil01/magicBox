package world.horosho.socketclientconnector;

import android.graphics.Color;
import android.os.Bundle;
import android.util.Log;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import java.util.LinkedList;
import java.util.Queue;

public class MainActivity extends AppCompatActivity implements UICallbacks{
    private Queue<String> queue = new LinkedList<>();
    private SocketService socketService;
    private boolean connectionInitiated = false;


    @Override
    protected void onCreate(Bundle savedInstanceState) {

        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_main);
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });

        socketService = new SocketService(this);
        Button actionBtn = findViewById(R.id.btn_action);
        Button sendMessage = findViewById(R.id.btn_send);
        sendMessage.setEnabled(connectionInitiated);

        TextView field = findViewById(R.id.textField);

        actionBtn.setOnClickListener(click -> {

            if (!connectionInitiated){
                //process connection
                String ip = field.getText().toString();
                Log.d("ipAddress", ip);
                if (ip.trim().isEmpty()){
                    Toast.makeText(this,
                            "Введите айпи адрес для соединения!", Toast.LENGTH_SHORT).show();
                    return;
                }

                if (socketService.connect(ip)){
                    Toast.makeText(this,
                            "ПОДКЛЮЧЕНО!", Toast.LENGTH_SHORT).show();
                    actionBtn.setText("DISCONNECT");
                    sendMessage.setEnabled(true);
                    connectionInitiated = true;
                    return;
                }
                Toast.makeText(this,
                        "НЕ УДАЛОСЬ ПОДКЛЮЧИТЬСЯ!!", Toast.LENGTH_SHORT).show();
            }else{
                //process disconnection
               if (socketService.disconnect()){
                   Toast.makeText(this,
                           "ОТКЛЮЧЕНО!", Toast.LENGTH_SHORT).show();
                   connectionInitiated = false;
                   sendMessage.setEnabled(false);
               }else{
                   Toast.makeText(this,
                           "НЕ УДАЛОСЬ ОТКЛЮЧИТЬСЯ!!", Toast.LENGTH_SHORT).show();
               }
            }

        });

        sendMessage.setOnClickListener(cl -> {

            String message = field.getText().toString();
            if (!message.trim().isEmpty()){
                socketService.sendMessage(message);
            }

        });


    }


    @Override
    public void populateRequests(String request) {
        runOnUiThread(() -> {

            StringBuilder sb = new StringBuilder();

            if (queue.size() == 5) {
                queue.poll();
            }

            queue.add(request);

            sb.append("ПОСЛЕДНИЕ 5 ЗАПРОСОВ").append("\n");
            queue.forEach(el -> {
                sb.append(el).append("\n");
                Log.d("stringBuilderEl", el);
            });

            Log.d("stringBuilder", sb.toString());

            ((TextView)findViewById(R.id.socket_history)).setText(sb.toString());

        });

    }

    @Override
    public void updateStatus(boolean status) {
        runOnUiThread(() -> {

            TextView socketStatus = findViewById(R.id.socket_status);
            if (status){
                socketStatus.setText("CONNECTED!!");
                socketStatus.setTextColor(Color.GREEN);
            }else{
                socketStatus.setText("DISCONNECTED");
                socketStatus.setTextColor(Color.RED);
            }

        });

    }
}