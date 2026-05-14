import torch
import torch.nn as nn
import torch.optim as optim
import numpy as np
import pandas as pd
import os

# Set device
device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')

# Load data
dir_path = os.path.dirname(os.path.abspath(__file__))
features_path = os.path.join(dir_path, 'features.csv')
labels_path = os.path.join(dir_path, 'labels.csv')

X = pd.read_csv(features_path, header=0).values
y = pd.read_csv(labels_path, header=0).values.flatten()

# Convert to tensors
X_tensor = torch.tensor(X, dtype=torch.float32).to(device)
y_tensor = torch.tensor(y, dtype=torch.float32).unsqueeze(1).to(device)

# Define Model
class BPNetwork(nn.Module):
    def __init__(self, input_dim):
        super(BPNetwork, self).__init__()
        self.fc1 = nn.Linear(input_dim, 8)
        self.relu = nn.ReLU()
        self.fc2 = nn.Linear(8, 1) # Binary classification logits
        
    def forward(self, x):
        out = self.fc1(x)
        out = self.relu(out)
        out = self.fc2(out)
        return out

input_dim = X.shape[1]
model = BPNetwork(input_dim).to(device)

criterion = nn.BCEWithLogitsLoss()
optimizer = optim.Adam(model.parameters(), lr=0.01)

# Train
epochs = 1000
print("Starting training...")
for epoch in range(epochs):
    model.train()
    optimizer.zero_grad()
    outputs = model(X_tensor)
    loss = criterion(outputs, y_tensor)
    loss.backward()
    optimizer.step()
    
    if (epoch + 1) % 100 == 0:
        print(f'Epoch [{epoch+1}/{epochs}], Loss: {loss.item():.4f}')

# Evaluate
model.eval()
with torch.no_grad():
    outputs = model(X_tensor)
    predictions = (torch.sigmoid(outputs) > 0.5).float()
    accuracy = (predictions == y_tensor).float().mean()
    print(f'Accuracy: {accuracy.item():.4f}')

# Save weights
model_path = os.path.join(dir_path, 'model.pth')
torch.save(model.state_dict(), model_path)
print(f'Model saved to {model_path}')

# Print weights for C
print("\nModel Weights for C porting:")
for name, param in model.named_parameters():
    print(f"--- {name} ---")
    data = param.data.cpu().numpy()
    if data.ndim == 1:
        print("{" + ", ".join(f"{x:.6f}" for x in data) + "}")
    elif data.ndim == 2:
        print("{")
        for row in data:
            print("  {" + ", ".join(f"{x:.6f}" for x in row) + "},")
        print("}")
