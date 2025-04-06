package world.horosho.socketclientconnector;

import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.nio.charset.Charset;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;
import okhttp3.WebSocket;
import okhttp3.WebSocketListener;
import okio.ByteString;

public class SocketService extends WebSocketListener {
    private final UICallbacks cb;
    private WebSocket webSocket;
    private OkHttpClient client;

    private String lastMessage;
    public SocketService(UICallbacks cb) {
        this.cb = cb;
    }

    public boolean connect(String ipAddress) {
        try {
            if (isValidIP(ipAddress)) {
                if (client == null) {
                    client = new OkHttpClient.Builder()
                            .readTimeout(0, TimeUnit.MILLISECONDS)
                            .build();
                }

                Request request = new Request.Builder()
                        .url("ws://" + ipAddress + ":9001")
                        .build();

                // Создаём WebSocket и сохраняем ссылку
                webSocket = client.newWebSocket(request, this);

                return true;
            }
            return false;
        } catch (Exception e) {
            Log.e("SocketService", "Ошибка подключения: " + e.getMessage());
            throw new RuntimeException(e);
        }
    }

    // Отключение от WebSocket
    public boolean disconnect() {
        if (webSocket != null) {
            webSocket.close(1000, "Клиент инициировал отключение");
            webSocket = null;
            cb.updateStatus(false);
        }
        if (client != null) {
            client.dispatcher().executorService().shutdown();
            client = null;
        }

        return true;
    }

    // Отправка сообщения через WebSocket
    public boolean sendMessage(String message) {
        if (webSocket != null) {
            lastMessage = message;
            return webSocket.send(message); // Возвращает true, если отправка успешна
        } else {
            Log.w("SocketService", "WebSocket не подключён");
            return false;
        }
    }


    private boolean isValidIP(String ipAddress) {
        if (ipAddress == null || ipAddress.isEmpty()) return false;

        String[] parts = ipAddress.split("\\.");
        if (parts.length != 4) return false;

        for (String part : parts) {
            try {
                if (part.isEmpty() || (part.length() > 1 && part.startsWith("0"))) return false;

                int number = Integer.parseInt(part);
                Log.d("validIp", String.valueOf(number));
                if (number < 0 || number > 255) return false;
            } catch (NumberFormatException e) {
                return false;
            }
        }
        return true;
    }

    // WebSocketListener методы
    @Override
    public void onOpen(@NonNull WebSocket webSocket, @NonNull Response response) {
        this.webSocket = webSocket; // Сохраняем ссылку на открытый WebSocket
        cb.updateStatus(true);
        Log.d("socketState", "Подключено: " + response.message());
    }

    @Override
    public void onMessage(@NonNull WebSocket webSocket, @NonNull String text) {
        Log.d("socketState", "Получено сообщение: " + text);
        DateTimeFormatter dtf = DateTimeFormatter.ofPattern("HH:mm:ss", Locale.ROOT);
        String timestamp = LocalDateTime.now().format(dtf);
        Log.d("currStamp", timestamp);
        cb.populateRequests("("+ timestamp + ") " + lastMessage + ": " + text);
    }

    @Override
    public void onMessage(@NonNull WebSocket webSocket, @NonNull ByteString bytes) {
        String message = bytes.string(Charset.defaultCharset());
        Log.d("socketState", "Получено байтовое сообщение: " + message);
        DateTimeFormatter dtf = DateTimeFormatter.ofPattern("HH:mm:ss", Locale.ROOT);
        String timestamp = LocalDateTime.now().format(dtf);
        Log.d("currStamp", timestamp);
        cb.populateRequests("("+ timestamp + ") " + lastMessage + ": " + message);
    }

    @Override
    public void onClosing(@NonNull WebSocket webSocket, int code, @NonNull String reason) {
        Log.d("socketState", "Закрывается: " + reason + " (код: " + code + ")");
        this.webSocket = null;
        cb.updateStatus(false);
    }

    @Override
    public void onClosed(@NonNull WebSocket webSocket, int code, @NonNull String reason) {
        Log.d("socketState", "Закрыто: " + reason + " (код: " + code + ")");
        this.webSocket = null;
        cb.updateStatus(false);
    }

    @Override
    public void onFailure(@NonNull WebSocket webSocket, @NonNull Throwable t, @Nullable Response response) {
        Log.e("socketState", "Ошибка: " + t.getMessage());
        this.webSocket = null;
        cb.updateStatus(false);
    }
}

