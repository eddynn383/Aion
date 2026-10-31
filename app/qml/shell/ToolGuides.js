.pragma library

// Ghidurile uneltelor: ce face unealta și cum se folosește, afișate la hover lung pe butonul ei
// (ca „rich tooltips” din Photoshop) și în turul aplicației. Animația corespunzătoare e în ToolDemo.qml.
const guides = {
    select: {
        title: qsTrId("ToolGuides.select"),
        summary: qsTrId("ToolGuides.alege_muchii_fete_grupuri_si_obiecte_ca_sa_lucrez"),
        steps: [qsTrId("ToolGuides.click_pe_o_fata_sau_o_muchie_o_selecteaza"), qsTrId("ToolGuides.dublu_click_fata_cu_muchiile_ei_triplu_click_tot_"),
                qsTrId("ToolGuides.trage_spre_dreapta_doar_ce_e_complet_in_dreptungh")],
        tip: qsTrId("ToolGuides.shift_adauga_sau_scoate_din_selectie")
    },
    line: {
        title: qsTrId("ToolGuides.line"),
        summary: qsTrId("ToolGuides.deseneaza_muchii_cand_liniile_inchid_un_contur_pl"),
        steps: [qsTrId("ToolGuides.click_pentru_primul_punct_apoi_click_pentru_urmat"), qsTrId("ToolGuides.urmeaza_inferentele_capat_mijloc_axele_colorate"),
                qsTrId("ToolGuides.tasteaza_lungimea_de_exemplu_3m_si_apasa_enter")],
        tip: qsTrId("ToolGuides.sagetile_blocheaza_directia_pe_o_axa_esc_opreste_")
    },
    arc: {
        title: qsTrId("ToolGuides.arc"),
        summary: qsTrId("ToolGuides.deseneaza_un_arc_din_doua_capete_si_o_sageata_cat"),
        steps: [qsTrId("ToolGuides.click_pe_primul_capat_apoi_pe_al_doilea"), qsTrId("ToolGuides.misca_mouse_ul_ca_sa_alegi_bombarea_apoi_click"),
                qsTrId("ToolGuides.tasteaza_sageata_sau_numarul_de_segmente_de_exemp")],
        tip: qsTrId("ToolGuides.arcul_e_o_singura_curba_eraser_il_sterge_intreg")
    },
    rectangle: {
        title: qsTrId("ToolGuides.rectangle"),
        summary: qsTrId("ToolGuides.deseneaza_un_dreptunghi_din_doua_colturi_opuse_fa"),
        steps: [qsTrId("ToolGuides.click_pentru_primul_colt"), qsTrId("ToolGuides.misca_mouse_ul_si_da_click_pentru_coltul_opus"),
                qsTrId("ToolGuides.sau_tasteaza_latimea_si_lungimea_3000_2000")],
        tip: qsTrId("ToolGuides.pe_o_fata_existenta_dreptunghiul_o_imparte")
    },
    board: {
        title: qsTrId("ToolGuides.placa_pe_fata"),
        summary: qsTrId("ToolGuides.deseneaza_o_placa_de_mobilier_pal_18_implicit_dir"),
        steps: [qsTrId("ToolGuides.click_pe_primul_colt_pe_o_fata_sau_pe_sol"), qsTrId("ToolGuides.click_pe_coltul_opus_sau_tasteaza_600_400"),
                qsTrId("ToolGuides.tipul_si_grosimea_placii_noi_se_aleg_in_inspector")],
        tip: qsTrId("ToolGuides.fibra_merge_pe_latura_mai_lunga_placa_apare_in_li")
    },
    stretch: {
        title: qsTrId("ToolGuides.intinde"),
        summary: qsTrId("ToolGuides.lateste_sau_inalta_un_corp_facut_din_placi_placil"),
        steps: [qsTrId("ToolGuides.click_pe_o_fata_a_corpului_axa_e_normala_ei"), qsTrId("ToolGuides.muta_planul_de_taiere_cu_mouse_ul_si_da_click"),
                qsTrId("ToolGuides.misca_mouse_ul_pentru_distanta_si_da_click_sau_ta")],
        tip: qsTrId("ToolGuides.un_corp_din_biblioteca_isi_schimba_doar_campul_o_")
    },
    circle: {
        title: qsTrId("ToolGuides.circle"),
        summary: qsTrId("ToolGuides.deseneaza_un_cerc_ca_poligon_cu_multe_laturi_cu_o"),
        steps: [qsTrId("ToolGuides.click_pentru_centru"), qsTrId("ToolGuides.misca_mouse_ul_pentru_raza_si_da_click"), qsTrId("ToolGuides.tasteaza_raza_sau_numarul_de_laturi_de_exemplu_48")],
        tip: qsTrId("ToolGuides.push_pull_pe_un_cerc_face_un_cilindru_neted")
    },
    polygon: {
        title: qsTrId("ToolGuides.polygon"),
        summary: qsTrId("ToolGuides.deseneaza_un_poligon_regulat_implicit_hexagon"),
        steps: [qsTrId("ToolGuides.click_pentru_centru"), qsTrId("ToolGuides.misca_mouse_ul_pentru_raza_si_da_click"), qsTrId("ToolGuides.tasteaza_numarul_de_laturi_urmat_de_s_8s")],
        tip: qsTrId("ToolGuides.muchiile_poligonului_raman_separate_spre_deosebir")
    },
    cube: {
        title: qsTrId("ToolGuides.cub"),
        summary: qsTrId("ToolGuides.creeaza_direct_un_obiect_cub_cu_fetele_lui_gata_d"),
        steps: [qsTrId("ToolGuides.click_pentru_centrul_bazei_pe_o_fata_in_planul_ei"), qsTrId("ToolGuides.misca_mouse_ul_pentru_latime_si_da_click"),
                qsTrId("ToolGuides.misca_mouse_ul_pentru_inaltime_se_prinde_in_drept"),
                qsTrId("ToolGuides.sau_tasteaza_latimea_1000_ori_latimea_si_inaltime")],
        tip: qsTrId("ToolGuides.dimensiunile_se_pot_schimba_apoi_din_inspector")
    },
    sphere: {
        title: qsTrId("ToolGuides.sfera"),
        summary: qsTrId("ToolGuides.creeaza_un_obiect_sfera_cu_suprafata_neteda_aseza"),
        steps: [qsTrId("ToolGuides.click_pentru_centrul_bazei"), qsTrId("ToolGuides.misca_mouse_ul_pentru_diametru_si_da_click"), qsTrId("ToolGuides.sau_tasteaza_diametrul")],
        tip: ""
    },
    cone: {
        title: qsTrId("ToolGuides.con"),
        summary: qsTrId("ToolGuides.creeaza_un_obiect_con_cu_baza_pe_planul_ales"),
        steps: [qsTrId("ToolGuides.click_pentru_centrul_bazei"), qsTrId("ToolGuides.misca_mouse_ul_pentru_latime_diametru_si_da_click"),
                qsTrId("ToolGuides.misca_mouse_ul_pentru_inaltime_si_da_click"), qsTrId("ToolGuides.sau_tasteaza_diametrul_si_inaltimea_600_1200")],
        tip: ""
    },
    cylinder: {
        title: qsTrId("ToolGuides.cilindru"),
        summary: qsTrId("ToolGuides.creeaza_un_obiect_cilindru_cu_baza_pe_planul_ales"),
        steps: [qsTrId("ToolGuides.click_pentru_centrul_bazei"), qsTrId("ToolGuides.misca_mouse_ul_pentru_latime_diametru_si_da_click"),
                qsTrId("ToolGuides.misca_mouse_ul_pentru_inaltime_si_da_click"), qsTrId("ToolGuides.sau_tasteaza_diametrul_si_inaltimea_600_1500")],
        tip: ""
    },
    pushpull: {
        title: qsTrId("ToolGuides.push_pull"),
        summary: qsTrId("ToolGuides.trage_o_fata_in_sus_sau_in_jos_si_transforma_form"),
        steps: [qsTrId("ToolGuides.click_pe_o_fata"), qsTrId("ToolGuides.misca_mouse_ul_pentru_inaltime_si_da_click"), qsTrId("ToolGuides.sau_tasteaza_distanta_1_5m_si_apasa_enter")],
        tip: qsTrId("ToolGuides.dublu_click_repeta_ultima_distanta_option_pastrea")
    },
    offset: {
        title: qsTrId("ToolGuides.offset"),
        summary: qsTrId("ToolGuides.copiaza_conturul_unei_fete_la_o_distanta_constant"),
        steps: [qsTrId("ToolGuides.click_pe_o_fata"), qsTrId("ToolGuides.misca_mouse_ul_spre_interior_sau_spre_exterior_ap"), qsTrId("ToolGuides.sau_tasteaza_distanta")],
        tip: qsTrId("ToolGuides.util_pentru_rame_borduri_si_pereti_cu_grosime")
    },
    followme: {
        title: qsTrId("ToolGuides.follow_me"),
        summary: qsTrId("ToolGuides.va_extruda_un_profil_de_a_lungul_unui_drum_muluri"),
        steps: [qsTrId("ToolGuides.in_curand")],
        tip: ""
    },
    move: {
        title: qsTrId("ToolGuides.move"),
        summary: qsTrId("ToolGuides.muta_selectia_sau_cu_option_o_copiaza"),
        steps: [qsTrId("ToolGuides.click_pe_un_punct_de_pornire"), qsTrId("ToolGuides.misca_de_a_lungul_unei_axe_si_da_click"), qsTrId("ToolGuides.tasteaza_distanta_dupa_copiere_x3_face_trei_copii")],
        tip: qsTrId("ToolGuides.mutand_o_muchie_sau_un_varf_formele_se_deformeaza")
    },
    rotate: {
        title: qsTrId("ToolGuides.rotate"),
        summary: qsTrId("ToolGuides.roteste_selectia_in_jurul_unui_punct_cu_raportoru"),
        steps: [qsTrId("ToolGuides.click_pe_centrul_rotatiei"), qsTrId("ToolGuides.click_pe_directia_de_referinta"), qsTrId("ToolGuides.roteste_si_da_click_sau_tasteaza_unghiul_30")],
        tip: qsTrId("ToolGuides.culoarea_raportorului_arata_planul_albastru_orizo")
    },
    scale: {
        title: qsTrId("ToolGuides.scale"),
        summary: qsTrId("ToolGuides.mareste_sau_micsoreaza_selectia_uniform_pe_doua_a"),
        steps: [qsTrId("ToolGuides.click_pe_un_maner_colt_uniform_mijlocul_unei_much"),
                qsTrId("ToolGuides.misca_mouse_ul_si_da_click"), qsTrId("ToolGuides.sau_tasteaza_factorul_2_0_5_ori_2_1_1_5_x_y_z")],
        tip: qsTrId("ToolGuides.punctul_opus_manerului_ramane_pe_loc_cu_option_sc")
    },
    tape: {
        title: qsTrId("ToolGuides.tape_measure"),
        summary: qsTrId("ToolGuides.masoara_distante_si_creeaza_ghidaje_pentru_desen_"),
        steps: [qsTrId("ToolGuides.click_pe_o_muchie_si_trage_apare_un_ghidaj_parale"), qsTrId("ToolGuides.click_pe_un_punct_ghidaj_punct"),
                qsTrId("ToolGuides.tasteaza_distanta_ghidajului")],
        tip: qsTrId("ToolGuides.ghidajele_participa_la_inferenta_le_stergi_din_ed")
    },
    protractor: {
        title: qsTrId("ToolGuides.protractor"),
        summary: qsTrId("ToolGuides.creeaza_ghidaje_la_un_unghi_dat"),
        steps: [qsTrId("ToolGuides.click_pe_varful_unghiului"), qsTrId("ToolGuides.click_pe_linia_de_referinta"), qsTrId("ToolGuides.roteste_si_da_click_sau_tasteaza_unghiul")],
        tip: ""
    },
    dimension: {
        title: qsTrId("ToolGuides.dimension"),
        summary: qsTrId("ToolGuides.adauga_cote_care_raman_in_model_si_se_actualizeaz"),
        steps: [qsTrId("ToolGuides.click_pe_primul_punct"), qsTrId("ToolGuides.click_pe_al_doilea_punct"), qsTrId("ToolGuides.trage_cota_in_afara_si_da_click")],
        tip: qsTrId("ToolGuides.unitatea_cotelor_se_schimba_din_preferinte")
    },
    text: {
        title: qsTrId("ToolGuides.text"),
        summary: qsTrId("ToolGuides.pune_o_nota_cu_sageata_pe_model"),
        steps: [qsTrId("ToolGuides.click_pe_punctul_de_notat"), qsTrId("ToolGuides.click_unde_vrei_eticheta"), qsTrId("ToolGuides.scrie_textul_si_apasa_enter")],
        tip: qsTrId("ToolGuides.dublu_click_pe_o_nota_o_editeaza")
    },
    section: {
        title: qsTrId("ToolGuides.section_plane"),
        summary: qsTrId("ToolGuides.taie_modelul_cu_un_plan_ca_sa_vezi_interiorul_pla"),
        steps: [qsTrId("ToolGuides.click_pe_o_fata_planul_se_asaza_paralel_cu_ea"), qsTrId("ToolGuides.muta_planul_din_bara_viewport_ului_iconita_sectiu"),
                qsTrId("ToolGuides.activeaza_sau_dezactiveaza_taietura_oricand")],
        tip: ""
    },
    eraser: {
        title: qsTrId("ToolGuides.eraser"),
        summary: qsTrId("ToolGuides.sterge_muchii_si_fetele_care_depind_de_ele"),
        steps: [qsTrId("ToolGuides.click_pe_o_muchie"), qsTrId("ToolGuides.sau_tine_apasat_si_trece_peste_mai_multe_muchii")],
        tip: qsTrId("ToolGuides.stergerea_unei_muchii_dintre_doua_fete_coplanare_")
    },
    paint: {
        title: qsTrId("ToolGuides.paint_bucket"),
        summary: qsTrId("ToolGuides.aplica_materiale_pe_fete_grupuri_si_obiecte"),
        steps: [qsTrId("ToolGuides.alege_un_material_din_panoul_materiale"), qsTrId("ToolGuides.click_pe_o_fata")],
        tip: qsTrId("ToolGuides.ia_materialul_de_pe_o_fata_inlocuieste_peste_tot_")
    },
    interact: {
        title: qsTrId("ToolGuides.interact"),
        summary: qsTrId("ToolGuides.deschide_si_inchide_usile_sertarele_si_clapetele_"),
        steps: [qsTrId("ToolGuides.selecteaza_un_grup_si_alege_in_entity_info_intera"),
                qsTrId("ToolGuides.alege_latura_balamalei_sau_directia_si_unghiul_sa"),
                qsTrId("ToolGuides.cu_interact_click_pe_obiect_se_deschide_inca_un_c")],
        tip: qsTrId("ToolGuides.starea_deschis_inchis_se_salveaza_in_fisier_si_ar")
    },
    orbit: {
        title: qsTrId("ToolGuides.orbit"),
        summary: qsTrId("ToolGuides.roteste_camera_in_jurul_modelului"),
        steps: [qsTrId("ToolGuides.trage_cu_mouse_ul"), qsTrId("ToolGuides.sau_oricand_butonul_din_mijloc_doua_degete_pe_tra")],
        tip: qsTrId("ToolGuides.gizmo_ul_din_colt_duce_la_vederile_standard")
    },
    pan: {
        title: qsTrId("ToolGuides.pan"),
        summary: qsTrId("ToolGuides.deplaseaza_vederea_fara_sa_o_rotesti"),
        steps: [qsTrId("ToolGuides.trage_cu_mouse_ul"), qsTrId("ToolGuides.sau_oricand_shift_butonul_din_mijloc")],
        tip: ""
    },
    zoom: {
        title: qsTrId("ToolGuides.zoom"),
        summary: qsTrId("ToolGuides.apropie_sau_departeaza_camera"),
        steps: [qsTrId("ToolGuides.trage_in_sus_sau_in_jos"), qsTrId("ToolGuides.sau_rotita_mouse_ului_pinch_pe_trackpad")],
        tip: qsTrId("ToolGuides.shift_z_incadreaza_tot_modelul")
    }
}

function guide(id) {
    return guides[id] || null
}

// Ordinea din turul aplicației.
const tourOrder = ["select", "line", "rectangle", "cube", "pushpull", "move", "rotate", "scale", "offset", "arc", "circle",
                   "polygon", "eraser", "paint", "tape", "protractor", "dimension", "text", "section", "interact", "orbit", "pan", "zoom"]
