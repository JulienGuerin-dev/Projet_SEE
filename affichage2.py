import tkinter as tk
import subprocess
import atexit
import re
import os
import numpy as np
from scipy.stats import norm
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

PROGRAMME_C = "./ultrason_control"
ARGUMENT = "infinit"  #argument pour que le programme tourne en continu
FICHIER_DATA = "distance.txt"
MAX_POINTS = 1000  # on stocke les 1000 dernières mesures
REFRESH_RATE = 15 # vitesse de rafraichissement

# couleurs
BG_COLOR = "#121212"
PANEL_COLOR = "#1e1e1e"
TEXT_COLOR = "#ffffff"
ACCENT_COLOR = "#00bfff"

distances = [] #tableau pour stocker les distances


try:
    # on lance le programme C
    process_c = subprocess.Popen([PROGRAMME_C, ARGUMENT])
except Exception as e:
    print(f"Erreur lancement C : {e}")
    process_c = None

def cleanup():
    if process_c:
        process_c.terminate()

atexit.register(cleanup) #arrete le programme si on ferme la fenêtre python


def fetch_distance():
    global distances
    try:
        if os.path.exists(FICHIER_DATA):
            # on ouvre et on lit le fichier 
            with open(FICHIER_DATA, "r") as f:
                content = f.read().strip()
                
            match = re.search(r'\d+(\.\d+)?', content) #cherche un nombre qui peut avoir des décimales et du texte derrière (cm dans notre cas)
            if match:
                val = float(match.group()) #cast de string en float
                
                
                distances.append(val) #on ajoute la valeur à la liste
                if len(distances) > MAX_POINTS: #si on dépasse les 1000 pointsz dans notre tableau on supprime le 1 er élément
                    distances.pop(0)


                label_dist.config(text=f"{val:.1f} cm", fg="#00ffcc")# on met à jour l'affichage
                
                if len(distances) > 1: # faut au moins 2 valeurs sinon ça n'a pas d'intéret
                    mean_val = np.mean(distances) #moyenne
                    std_val = np.std(distances) #écart-type
                    label_stats.config(text=f"Moyenne : {mean_val:.2f} cm\nÉcart-type : {std_val:.2f} cm")
                    update_plot(mean_val, std_val)
        else:
            label_dist.config(text="Fichier absent", fg="orange")
    except Exception:
        label_dist.config(text="Erreur lecture", fg="#ff4444") #try catch comme en C
    
    root.after(REFRESH_RATE, fetch_distance) #relance le programme du dessus

def update_plot(mean, std):
    if std == 0: return
    ax.clear()
    ax.set_facecolor(PANEL_COLOR)
    ax.tick_params(colors=TEXT_COLOR)
    
    # l'histogramme des données
    ax.hist(distances, bins=20, density=True, alpha=0.7, color=ACCENT_COLOR, edgecolor='white')
    
    # gaussienne
    xmin, xmax = ax.get_xlim()
    x = np.linspace(xmin, xmax, 100)
    p = norm.pdf(x, mean, std)
    ax.plot(x, p, 'r-', linewidth=2.5)
    
    ax.set_title(f"Analyse sur {len(distances)} mesures", color=TEXT_COLOR)
    canvas.draw()

# on utilise tkinter pour faire la fenêtre et la partie graphique dedans
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
