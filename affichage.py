import tkinter as tk
import subprocess
import atexit
import re
import os
import numpy as np
from scipy.stats import norm
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

# --- CONFIGURATION ---
PROGRAMME_C = "./ultrason_control"
ARGUMENT = "infinit"  # ton argument
FICHIER_DATA = "distance.txt"
MAX_POINTS = 1000  # Stockage des 1000 dernières mesures
REFRESH_RATE = 15 # Lecture toutes les 100ms

# Style
BG_COLOR = "#121212"
PANEL_COLOR = "#1e1e1e"
TEXT_COLOR = "#ffffff"
ACCENT_COLOR = "#00bfff"

distances = []

# --- GESTION DU PROCESSUS C ---
try:
    # Lancement du programme C (il écrit dans le fichier en continu)
    process_c = subprocess.Popen([PROGRAMME_C, ARGUMENT])
except Exception as e:
    print(f"Erreur lancement C : {e}")
    process_c = None

def cleanup():
    if process_c:
        process_c.terminate()

atexit.register(cleanup)

# --- LOGIQUE DE TRAITEMENT ---
def fetch_distance():
    global distances
    try:
        if os.path.exists(FICHIER_DATA):
            # Lecture de la valeur unique présente dans le fichier
            with open(FICHIER_DATA, "r") as f:
                content = f.read().strip()
                
            match = re.search(r'\d+(\.\d+)?', content)
            if match:
                val = float(match.group())
                
                # Stockage de la valeur dans la liste pour la Gaussienne
                distances.append(val)
                if len(distances) > MAX_POINTS:
                    distances.pop(0)

                # Mise à jour de l'affichage
                label_dist.config(text=f"{val:.1f} cm", fg="#00ffcc")
                
                if len(distances) > 1:
                    mean_val = np.mean(distances)
                    std_val = np.std(distances)
                    label_stats.config(text=f"Moyenne : {mean_val:.2f} cm\nÉcart-type : {std_val:.2f} cm")
                    update_plot(mean_val, std_val)
        else:
            label_dist.config(text="Fichier absent", fg="orange")
    except Exception:
        label_dist.config(text="Erreur lecture", fg="#ff4444")
    
    root.after(REFRESH_RATE, fetch_distance)

def update_plot(mean, std):
    if std == 0: return
    ax.clear()
    ax.set_facecolor(PANEL_COLOR)
    ax.tick_params(colors=TEXT_COLOR)
    
    # Histogramme des données stockées
    ax.hist(distances, bins=20, density=True, alpha=0.7, color=ACCENT_COLOR, edgecolor='white')
    
    # Courbe de Gauss théorique
    xmin, xmax = ax.get_xlim()
    x = np.linspace(xmin, xmax, 100)
    p = norm.pdf(x, mean, std)
    ax.plot(x, p, 'r-', linewidth=2.5)
    
    ax.set_title(f"Analyse sur {len(distances)} mesures", color=TEXT_COLOR)
    canvas.draw()

# --- INTERFACE TKINTER ---
root = tk.Tk()
root.title("Radar STM32")
root.geometry("800x450")
root.configure(bg=BG_COLOR)

frame_left = tk.Frame(root, bg=PANEL_COLOR, padx=20, pady=20)
frame_left.pack(side=tk.LEFT, fill=tk.Y, padx=10, pady=10)

tk.Label(frame_left, text="VALEUR ACTUELLE", font=("Arial", 10), bg=PANEL_COLOR, fg="#888888").pack()
label_dist = tk.Label(frame_left, text="---", font=("Consolas", 35, "bold"), bg=PANEL_COLOR, fg=TEXT_COLOR)
label_dist.pack(pady=20)

label_stats = tk.Label(frame_left, text="Moyenne : --\nÉcart-type : --", font=("Consolas", 11), bg=PANEL_COLOR, fg="#bbbbbb", justify=tk.LEFT)
label_stats.pack()

frame_right = tk.Frame(root, bg=BG_COLOR)
frame_right.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=10, pady=10)

fig = Figure(figsize=(5, 4), dpi=100, facecolor=BG_COLOR)
ax = fig.add_subplot(111)
canvas = FigureCanvasTkAgg(fig, master=frame_right)
canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

fetch_distance()
root.mainloop()
