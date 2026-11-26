"""Build and train the gesture CNN. Dataset loading and export are supplied."""
from training_support import run_training


# Build a four-class classifier for normalized 64 by 64 grayscale inputs.
def build_model(tf):
    return tf.keras.Sequential([
        tf.keras.layers.Input(shape=(64, 64, 1)),
        # Extract local features, then reduce image size after each convolution.
        tf.keras.layers.Conv2D(8, 3, padding="same", activation="relu"),
        tf.keras.layers.MaxPooling2D(2),
        tf.keras.layers.Conv2D(16, 3, padding="same", activation="relu"),
        tf.keras.layers.MaxPooling2D(2),
        tf.keras.layers.Conv2D(24, 3, padding="same", activation="relu"),
        tf.keras.layers.MaxPooling2D(2),
        # Turn spatial features into a compact classifier for the four labels.
        tf.keras.layers.Flatten(),
        tf.keras.layers.Dense(32, activation="relu"),
        # Drop some activations during training to reduce overfitting.
        tf.keras.layers.Dropout(0.2),
        tf.keras.layers.Dense(4, activation="softmax")
    ])


# Train with integer class labels and track validation performance.
def train_model(tf, model, x_train, y_train, x_val, y_val, epochs, batch_size):
    model.compile(
        optimizer="adam",
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"]
    )

    # Stop when validation loss stops improving and restore the best weights.
    early_stop = tf.keras.callbacks.EarlyStopping(
        monitor="val_loss",
        patience=8,
        restore_best_weights=True
    )

    # Use the separate validation split to monitor each training epoch.
    return model.fit(
        x_train, y_train,
        validation_data=(x_val, y_val),
        epochs=epochs,
        batch_size=batch_size,
        callbacks=[early_stop]
    )


# Use the shared loader, evaluator, and export pipeline.
if __name__ == '__main__':
    run_training(build_model, train_model)
